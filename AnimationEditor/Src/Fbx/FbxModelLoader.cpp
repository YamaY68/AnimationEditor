#include "FbxModelLoader.h"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <DxLib.h>
#include "../Common/EditorConstants.h"
using namespace DirectX;

// FbxAMatrix を DirectX::XMFLOAT4X4 へ変換
static XMFLOAT4X4 ToXMFLOAT4X4(const FbxAMatrix& src)
{
    XMFLOAT4X4 dst;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            dst.m[r][c] = static_cast<float>(src.Get(r, c));
        }
    }
    return dst;
}

// FBX SDK が .fbm へ展開したテクスチャをロードしてハンドルを返す
static int LoadFbxNodeTexture(FbxNode* node, std::unordered_map<FbxVideo*, int>& cache)
{
    if (!node) return -1;

    int matCount = node->GetMaterialCount();
    for (int m = 0; m < matCount; ++m)
    {
        FbxSurfaceMaterial* mat = node->GetMaterial(m);
        if (!mat) continue;

        FbxProperty prop = mat->FindProperty(FbxSurfaceMaterial::sDiffuse);
        if (!prop.IsValid()) continue;

        FbxFileTexture* fileTex = nullptr;
        if (prop.GetSrcObjectCount<FbxLayeredTexture>() > 0)
        {
            FbxLayeredTexture* layered = prop.GetSrcObject<FbxLayeredTexture>(0);
            if (layered && layered->GetSrcObjectCount<FbxFileTexture>() > 0)
            {
                fileTex = layered->GetSrcObject<FbxFileTexture>(0);
            }
        }
        else if (prop.GetSrcObjectCount<FbxFileTexture>() > 0)
        {
            fileTex = prop.GetSrcObject<FbxFileTexture>(0);
        }

        if (!fileTex) continue;

        FbxVideo* video = fileTex->GetSrcObject<FbxVideo>(0);
        if (!video) continue;

        // 既にロード済みならキャッシュを返す
        auto it = cache.find(video);
        if (it != cache.end())
        {
            return it->second;
        }

        // ★重要：FbxString を一時変数に受けて寿命を延ばしてから文字列を取り出す（文字化け防止）
        FbxString fileNameStr = video->GetFileName();
        std::string extractedPath = fileNameStr.Buffer();

        if (!extractedPath.empty() && std::filesystem::exists(extractedPath))
        {
            int handle = LoadGraph(extractedPath.c_str());
            if (handle != -1)
            {
                cache[video] = handle;
                return handle;
            }
        }
    }

    return -1;
}
FbxModelLoader::FbxModelLoader(void)
{
}

FbxModelLoader::~FbxModelLoader(void)
{
    Finalize();
}

bool FbxModelLoader::Initialize(void)
{
    if (!fbxManager_)
    {
        fbxManager_ = FbxManager::Create();
        if (!fbxManager_) return false;

        ioSetting_ = FbxIOSettings::Create(fbxManager_, IOSROOT);
        ioSetting_->SetBoolProp(IMP_FBX_EXTRACT_EMBEDDED_DATA, true);
        fbxManager_->SetIOSettings(ioSetting_);
    }
    return true;
}

void FbxModelLoader::Finalize(void)
{
    if (fbxScene_)
    {
        fbxScene_->Destroy();
        fbxScene_ = nullptr;
    }
    if (ioSetting_)
    {
        ioSetting_->Destroy();
        ioSetting_ = nullptr;
    }
    if (fbxManager_)
    {
        fbxManager_->Destroy();
        fbxManager_ = nullptr;
    }
}

/// FBXファイルからボーン階層を構築する
bool FbxModelLoader::LoadSkeleton(const std::string& filePath, Skeleton& outSkeleton)
{
    if (!Initialize()) return false;
    //インポーターを作成してFBXファイルを開く
    fbxsdk::FbxImporter* sdkImporter = fbxsdk::FbxImporter::Create(fbxManager_, "");
    if (!sdkImporter->Initialize(filePath.c_str(), -1, ioSetting_))
    {
        sdkImporter->Destroy();
        return false;
    }
    //前回のシーンが残っていれば新しいシーンへインポートする
    if (fbxScene_) fbxScene_->Destroy();
    fbxScene_ = FbxScene::Create(fbxManager_, "ImportScene");
    sdkImporter->Import(fbxScene_);
    sdkImporter->Destroy();
    //シーンのルートノードから深さ優先探索でボーン階層を構築
    FbxNode* rootNode = fbxScene_->GetRootNode();
    if (rootNode)
    {
        for (int i = 0; i < rootNode->GetChildCount(); ++i)
        {
            ProcessSkeletonHierarchy(rootNode->GetChild(i), -1, outSkeleton);
        }
    }
    return outSkeleton.GetBoneCount() > 0;
}

bool FbxModelLoader::LoadMeshWithSkinning(const std::string& filepath, Skeleton& skeleton, FbxMeshModel& outMeshModel)
{
    if (!fbxScene_) return false;
    //シーン内の全ポリゴンを三角形に分割する
    FbxGeometryConverter geomConverter(fbxManager_);
    geomConverter.Triangulate(fbxScene_, true);
    //シーンツリー全体からFbxMeshを持つノードをすべて収集する
    std::vector<FbxNode*> meshNodes;
    std::function<void(FbxNode*)> findMeshes = [&](FbxNode* n)
        {
            if (!n) return;
            if (n->GetMesh()) meshNodes.push_back(n);
            for (int i = 0; i < n->GetChildCount(); ++i) findMeshes(n->GetChild(i));
        };
    findMeshes(fbxScene_->GetRootNode());

    if (meshNodes.empty()) return false;

    outMeshModel.ReleaseTextures();
    outMeshModel.sourceVertices_.clear();
    outMeshModel.subMeshes_.clear();
    //同一の埋め込みテクスチャを複数パーツで使いまわすためのキャッシュ
    std::unordered_map<FbxVideo*, int> loadedTextures;

    for (FbxNode* node : meshNodes)
    {
        FbxMesh* mesh = node->GetMesh();
        if (!mesh) continue;
        //メッシュノード自身のバインド時ワールド変換行列
        FbxAMatrix meshGlobal = node->EvaluateGlobalTransform(0);
        //コントロールポイント
        int cpCount = mesh->GetControlPointsCount();
        FbxVector4* controlPoints = mesh->GetControlPoints();
        //コントロールポイント番号ごとに最大４ボーンのインデックスと重みを一時保存する配列
        struct VertexWeightInfo
        {
            int indices[4] = { -1, -1, -1, -1 };
            float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        };
        std::vector<VertexWeightInfo> cpWeights(cpCount);
        //スキンでフォーマからボーンウェイトと逆バインド行列を摘出
        int skinCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
        for (int s = 0; s < skinCount; ++s)
        {
            FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(s, FbxDeformer::eSkin));
            int clusterCount = skin->GetClusterCount();

            for (int c = 0; c < clusterCount; ++c)
            {
                FbxCluster* cluster = skin->GetCluster(c);
                FbxNode* linkNode = cluster->GetLink();
                if (!linkNode) continue;

                int boneIdx = skeleton.FindBoneIndex(linkNode->GetName());
                if (boneIdx < 0) continue;

                //バインド時のボーンのグローバル行列を取得し、その逆行列を逆バインド行列としてボーンに保存
                FbxAMatrix transformLinkMatrix;
                cluster->GetTransformLinkMatrix(transformLinkMatrix);

                XMFLOAT4X4 bindMat = ToXMFLOAT4X4(transformLinkMatrix);
                XMMATRIX xBind = XMLoadFloat4x4(&bindMat);
                XMVECTOR det;
                XMMATRIX xInvBind = XMMatrixInverse(&det, xBind);
                XMStoreFloat4x4(&const_cast<Bone&>(skeleton.GetBone(boneIdx)).invBindMatrix_, xInvBind);
                //このボーンが影響を与えるコントロールポイント番号リストとウェイト値リストを取得
                int indexCount = cluster->GetControlPointIndicesCount();
                int* cpIndices = cluster->GetControlPointIndices();
                double* weights = cluster->GetControlPointWeights();

                for (int k = 0; k < indexCount; ++k)
                {
                    int cpIdx = cpIndices[k];
                    float w = static_cast<float>(weights[k]);
                    if (cpIdx < 0 || cpIdx >= cpCount) continue;
                    //ウエイト値が大きい順に上位４つまでを挿入ソートで保存する
                    for (int slot = 0; slot < 4; ++slot)
                    {
                        if (w > cpWeights[cpIdx].weights[slot])
                        {
                            for (int shift = 3; shift > slot; --shift)
                            {
                                cpWeights[cpIdx].weights[shift] = cpWeights[cpIdx].weights[shift - 1];
                                cpWeights[cpIdx].indices[shift] = cpWeights[cpIdx].indices[shift - 1];
                            }
                            cpWeights[cpIdx].weights[slot] = w;
                            cpWeights[cpIdx].indices[slot] = boneIdx;
                            break;
                        }
                    }
                }
            }
        }
        //UVセット名の取得と植え込みテクスチャのロード
        const char* uvSetName = nullptr;
        if (mesh->GetElementUVCount() > 0)
        {
            uvSetName = mesh->GetElementUV(0)->GetName();
        }

        int texHandle = LoadFbxNodeTexture(node, loadedTextures);

        //サブメッシュ情報を記録
        SubMesh subMesh;
        subMesh.startIndex_ = static_cast<int>(outMeshModel.sourceVertices_.size());
        subMesh.textureHandle_ = texHandle;
        //ポリゴン単位で頂点を展開して格納
        int polygonCount = mesh->GetPolygonCount();
        // DxLib（左手系）用に時計回り（0, 2, 1）で頂点を展開して格納
        const int winding[3] = { 0, 2, 1 };

        for (int p = 0; p < polygonCount; ++p)
        {
            for (int w = 0; w < 3; ++w)
            {
                int v = winding[w];
                int cpIndex = mesh->GetPolygonVertex(p, v);

                SkinVertex vert;
                FbxVector4 pos = meshGlobal.MultT(controlPoints[cpIndex]);
                vert.position_ = { static_cast<float>(pos[0]), static_cast<float>(pos[1]), static_cast<float>(pos[2]) };                //ポリゴン頂点ごとの法線を取得
                FbxVector4 norm;
                if (mesh->GetPolygonVertexNormal(p, v, norm))
                {
                    vert.normal_ = { static_cast<float>(norm[0]), static_cast<float>(norm[1]), static_cast<float>(norm[2]) };
                }
                //UV座標を取得
                if (uvSetName)
                {
                    FbxVector2 uv;
                    bool unmapped;
                    if (mesh->GetPolygonVertexUV(p, v, uvSetName, uv, unmapped))
                    {
                        vert.uv_ = { static_cast<float>(uv[0]), 1.0f - static_cast<float>(uv[1]) };
                    }
                }
                //コントロールポイントに紐づけておいた最大４ボーンのウェイトをコピー
                for (int k = 0; k < 4; ++k)
                {
                    vert.boneWeights_[k] = cpWeights[cpIndex].weights[k];
                    vert.boneIndices_[k] = cpWeights[cpIndex].indices[k];
                }
                //ウェイトの合計値が必ず1.0fになるように正規化
                float totalWeight = vert.boneWeights_[0] + vert.boneWeights_[1] + vert.boneWeights_[2] + vert.boneWeights_[3];
                if (totalWeight > 0.0001f)
                {
                    for (int k = 0; k < 4; ++k) vert.boneWeights_[k] /= totalWeight;
                }

                outMeshModel.sourceVertices_.push_back(vert);
            }
        }

        subMesh.indexCount_ = static_cast<int>(polygonCount * 3);
        outMeshModel.subMeshes_.push_back(subMesh);
    }

    return !outMeshModel.sourceVertices_.empty();
}

bool FbxModelLoader::LoadAnimation(const std::string& filePath, const Skeleton& skeleton, AnimationClip& outClip)
{
    if (!Initialize()) return false;

    fbxsdk::FbxImporter* sdkImporter = fbxsdk::FbxImporter::Create(fbxManager_, "");
    if (!sdkImporter->Initialize(filePath.c_str(), -1, ioSetting_))
    {
        sdkImporter->Destroy();
        return false;
    }
    //アニメーション読み込み用の一時シーンを作成
    FbxScene* scene = FbxScene::Create(fbxManager_, "AnimScene");
    sdkImporter->Import(scene);
    sdkImporter->Destroy();
    //シーン内にアニメーションデータがなければ終了
    int animStackCount = scene->GetSrcObjectCount<FbxAnimStack>();
    if (animStackCount == 0)
    {
        scene->Destroy();
        return false;
    }
    //０番目のアニメスタックをアクティブに設定しないとEvaluteLocalTransformが常に静止ポーズを返す
    FbxAnimStack* animStack = scene->GetSrcObject<FbxAnimStack>(0);
    scene->SetCurrentAnimationStack(animStack);

    outClip.name_ = animStack->GetName();
    //アニメーションの開始時刻の長さを取得
    FbxTimeSpan timeSpan = animStack->GetLocalTimeSpan();
    FbxTime startTime = timeSpan.GetStart();
    float duration = static_cast<float>(timeSpan.GetDuration().GetSecondDouble());
    //LoaclTimeSpanが空のFBXの場合はTakeInfoやグローバルタイムラインから長さを取得する
    if (duration <= 0.05f)
    {
        FbxTakeInfo* takeInfo = scene->GetTakeInfo(animStack->GetName());
        if (takeInfo)
        {
            startTime = takeInfo->mLocalTimeSpan.GetStart();
            duration = static_cast<float>(takeInfo->mLocalTimeSpan.GetDuration().GetSecondDouble());
        }
        else
        {
            scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(timeSpan);
            startTime = timeSpan.GetStart();
            duration = static_cast<float>(timeSpan.GetDuration().GetSecondDouble());
        }
    }

    outClip.duration_ = duration;
    outClip.boneTracks_.clear();
    outClip.boneTracks_.reserve(skeleton.GetBoneCount());

    float startSec = static_cast<float>(startTime.GetSecondDouble());
    //親スケルトンの全ボーンについて同名ノードをアニメシーンから探してサンプリング
    for (size_t b = 0; b < skeleton.GetBoneCount(); ++b)
    {
        const auto& bone = skeleton.GetBone(b);
        FbxNode* node = scene->FindNodeByName(bone.name_.c_str());
        if (node)
        {
            BoneTrack* track = outClip.GetOrCreateTrack(static_cast<int>(b));
            track->boneIndex_ = static_cast<int>(b);
            //１秒間に30回の等間隔で姿勢をベイクして取り出す
            int frameCount = static_cast<int>(duration * EditorConst::kSampleFrameRate);
            float dt = EditorConst::kFrameDuration;

            for (int f = 0; f <= frameCount; ++f)
            {
                float t = (f == frameCount) ? duration : (f * dt);
                FbxTime fbxTime;
                fbxTime.SetSecondDouble(startSec + t);
                //全ボーン統一でローカル変換行列を評価
                FbxAMatrix localMatrix = node->EvaluateLocalTransform(fbxTime);

                FbxVector4 lT = localMatrix.GetT();
                FbxQuaternion lQ = localMatrix.GetQ();
                FbxVector4 lS = localMatrix.GetS();

                XMFLOAT3 pos = { static_cast<float>(lT[0]), static_cast<float>(lT[1]), static_cast<float>(lT[2]) };
                XMFLOAT4 rot = { static_cast<float>(lQ[0]), static_cast<float>(lQ[1]), static_cast<float>(lQ[2]), static_cast<float>(lQ[3]) };
                XMFLOAT3 scl = { static_cast<float>(lS[0]), static_cast<float>(lS[1]), static_cast<float>(lS[2]) };

                track->translationTrack_.AddKey(t, pos, InterpolationType::Linear);
                track->rotationTrack_.AddKey(t, rot, InterpolationType::Linear);
                track->scaleTrack_.AddKey(t, scl, InterpolationType::Linear);
            }
        }
    }

    scene->Destroy();
    return !outClip.boneTracks_.empty();
}

void FbxModelLoader::ProcessSkeletonHierarchy(FbxNode* node, int parentIndex, Skeleton& outSkeleton)
{
    if (!node) return;

    FbxNodeAttribute* attr = node->GetNodeAttribute();
    int currentBoneIndex = parentIndex;

    if (attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
    {
        std::string boneName = node->GetName();

        //０フレーム目のローカル変換行列を取得
        FbxAMatrix localMatrix = node->EvaluateLocalTransform(0);

        FbxVector4 lT = localMatrix.GetT();
        FbxQuaternion lQ = localMatrix.GetQ();
        FbxVector4 lS = localMatrix.GetS();

        XMFLOAT3 pos = { static_cast<float>(lT[0]), static_cast<float>(lT[1]), static_cast<float>(lT[2]) };
        XMFLOAT4 rot = { static_cast<float>(lQ[0]), static_cast<float>(lQ[1]), static_cast<float>(lQ[2]), static_cast<float>(lQ[3]) };
        XMFLOAT3 scl = { static_cast<float>(lS[0]), static_cast<float>(lS[1]), static_cast<float>(lS[2]) };

        Transform localBindPose(pos, rot, scl);
        //自分のインデックスを取得し、子ノード探索時の親インデックスとして渡す
        currentBoneIndex = outSkeleton.AddBone(boneName, parentIndex, localBindPose);
    }
    //子ノードの深さ優先探索
    for (int i = 0; i < node->GetChildCount(); ++i)
    {
        ProcessSkeletonHierarchy(node->GetChild(i), currentBoneIndex, outSkeleton);
    }
}