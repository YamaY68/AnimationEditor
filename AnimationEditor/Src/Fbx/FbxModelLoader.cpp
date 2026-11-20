#include "FbxModelLoader.h"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <system_error>
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

// UTF-8の文字列(FBX SDKが返すもの)を、Shift-JIS(DxLibに渡すもの)に変換する
static std::string Utf8ToAnsi(const char* utf8)
{
    char* ansi = nullptr;
    FbxUTF8ToAnsi(utf8, ansi);	// FBX SDK付属の変換関数(変換後の文字列はFbxFreeで解放する)
    std::string result = ansi ? ansi : "";
    FbxFree(ansi);
    return result;
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

        //FbxStringを一時変数に受けてから文字列を取り出す(一時オブジェクトのまま使うと、文の終わりで消えるため)
        FbxString fileNameStr = video->GetFileName();
        const char* utf8Path = fileNameStr.Buffer();
        if (utf8Path == nullptr || utf8Path[0] == '\0') continue;

        try
        {
            //FBX SDKのパスはUTF-8なので、char8_tとして渡してUTF-8だと伝える
            //(std::stringのまま渡すと、Shift-JISとして変換されて、日本語などが入っていると例外になる)
            std::error_code ec;
            const std::filesystem::path texPath(reinterpret_cast<const char8_t*>(utf8Path));
            if (!std::filesystem::exists(texPath, ec)) continue;
        }
        catch (const std::exception&)
        {
            //それでも変換できないパス(壊れた文字列など)は、テクスチャなしとして扱う
            continue;
        }

        //DxLibはShift-JISでパスを受け取るので、変換してから渡す
        int handle = LoadGraph(Utf8ToAnsi(utf8Path).c_str());
        if (handle != -1)
        {
            cache[video] = handle;
            return handle;
        }
    }

    return -1;
}


// FbxAMatrix(ローカル行列)を、移動・回転・スケールに分けてTransformにする
static Transform ToTransform(const FbxAMatrix& m)
{
    FbxVector4 t = m.GetT();
    FbxQuaternion q = m.GetQ();
    FbxVector4 s = m.GetS();
    return Transform(
        XMFLOAT3(static_cast<float>(t[0]), static_cast<float>(t[1]), static_cast<float>(t[2])),
        XMFLOAT4(static_cast<float>(q[0]), static_cast<float>(q[1]), static_cast<float>(q[2]), static_cast<float>(q[3])),
        XMFLOAT3(static_cast<float>(s[0]), static_cast<float>(s[1]), static_cast<float>(s[2])));
}

// AnimStackの開始時刻と長さ(秒)を求める
// FBXによって長さの書き込み場所が違うので、AnimStack → TakeInfo → シーン全体のタイムライン の順に探す
static void GetAnimStackTimeSpan(FbxScene* scene, FbxAnimStack* animStack, FbxTime& outStart, float& outDuration)
{
    //これより短い長さは「記録されていない」とみなす
    constexpr float kMinValidDuration = 0.05f;

    FbxTimeSpan timeSpan = animStack->GetLocalTimeSpan();
    outStart = timeSpan.GetStart();
    outDuration = static_cast<float>(timeSpan.GetDuration().GetSecondDouble());
    if (outDuration > kMinValidDuration) return;

    if (FbxTakeInfo* takeInfo = scene->GetTakeInfo(animStack->GetName()))
    {
        outStart = takeInfo->mLocalTimeSpan.GetStart();
        outDuration = static_cast<float>(takeInfo->mLocalTimeSpan.GetDuration().GetSecondDouble());
        if (outDuration > kMinValidDuration) return;
    }

    scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(timeSpan);
    outStart = timeSpan.GetStart();
    outDuration = static_cast<float>(timeSpan.GetDuration().GetSecondDouble());
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

FbxScene* FbxModelLoader::ImportScene(const std::string& filePath, const char* sceneName)
{
    if (!Initialize()) return nullptr;

    fbxsdk::FbxImporter* importer = fbxsdk::FbxImporter::Create(fbxManager_, "");
    if (!importer->Initialize(filePath.c_str(), -1, ioSetting_))
    {
        importer->Destroy();
        return nullptr;
    }

    FbxScene* scene = FbxScene::Create(fbxManager_, sceneName);
    const bool imported = importer->Import(scene);
    importer->Destroy();

    if (!imported)
    {
        scene->Destroy();
        return nullptr;
    }
    return scene;
}

/// FBXファイルからボーン階層を構築する
bool FbxModelLoader::LoadSkeleton(const std::string& filePath, Skeleton& outSkeleton)
{
    FbxScene* scene = ImportScene(filePath, "ImportScene");
    if (!scene) return false;

    //前回のシーンを破棄して入れ替える(メッシュと埋め込みアニメーションの読み込みで使う)
    if (fbxScene_) fbxScene_->Destroy();
    fbxScene_ = scene;

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
        //メッシュのバインド時(スキンを付けたとき)のワールド行列
        //スキンがあれば、スキンを付けたときに記録された行列(クラスタのTransformMatrix)を使う
        //無ければ、アニメーションを含まない初期状態の行列を使う
        //(フレーム0の行列を使うと、モデルにアニメーションが入っているときに、その姿勢で頂点が固定されてしまう)
        FbxAMatrix meshGlobal = node->EvaluateGlobalTransform(FBXSDK_TIME_INFINITE);
        if (mesh->GetDeformerCount(FbxDeformer::eSkin) > 0)
        {
            FbxSkin* firstSkin = static_cast<FbxSkin*>(mesh->GetDeformer(0, FbxDeformer::eSkin));
            if (firstSkin && firstSkin->GetClusterCount() > 0)
            {
                firstSkin->GetCluster(0)->GetTransformMatrix(meshGlobal);
            }
        }
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

int FbxModelLoader::LoadAnimations(const std::string& filePath, const Skeleton& skeleton, std::vector<AnimationClip>& outClips)
{
    //アニメーション読み込み用の一時シーン(モデルのシーンとは別に開く)
    FbxScene* scene = ImportScene(filePath, "AnimScene");
    if (!scene) return 0;

    const int count = SampleAllAnimStacks(scene, skeleton, outClips);
    scene->Destroy();
    return count;
}

int FbxModelLoader::LoadEmbeddedAnimations(const Skeleton& skeleton, std::vector<AnimationClip>& outClips)
{
    if (!fbxScene_) return 0;
    return SampleAllAnimStacks(fbxScene_, skeleton, outClips);
}

int FbxModelLoader::SampleAllAnimStacks(FbxScene* scene, const Skeleton& skeleton, std::vector<AnimationClip>& outClips)
{
    int count = 0;
    const int animStackCount = scene->GetSrcObjectCount<FbxAnimStack>();
    for (int i = 0; i < animStackCount; ++i)
    {
        AnimationClip clip;
        if (SampleAnimStack(scene, scene->GetSrcObject<FbxAnimStack>(i), skeleton, clip))
        {
            outClips.push_back(std::move(clip));
            ++count;
        }
    }
    return count;
}

bool FbxModelLoader::SampleAnimStack(FbxScene* scene, FbxAnimStack* animStack, const Skeleton& skeleton, AnimationClip& outClip)
{
    if (!animStack) return false;

    //このAnimStackを有効にする(有効にしないと、EvaluateLocalTransformがアニメーションの無い姿勢を返す)
    scene->SetCurrentAnimationStack(animStack);

    FbxTime startTime;
    float duration = 0.0f;
    GetAnimStackTimeSpan(scene, animStack, startTime, duration);
    if (duration <= 0.0f) return false;	//長さの無いAnimStackは、アニメーションとして扱わない

    outClip.name_ = animStack->GetName();
    outClip.duration_ = duration;
    outClip.boneTracks_.clear();
    outClip.boneTracks_.reserve(skeleton.GetBoneCount());

    const double startSec = startTime.GetSecondDouble();
    const int frameCount = static_cast<int>(duration * EditorConst::kSampleFrameRate);

    //スケルトンの全ボーンについて、同じ名前のノードを探してサンプリングする
    for (size_t b = 0; b < skeleton.GetBoneCount(); ++b)
    {
        FbxNode* node = scene->FindNodeByName(skeleton.GetBone(b).name_.c_str());
        if (!node) continue;

        BoneTrack* track = outClip.GetOrCreateTrack(static_cast<int>(b));

        //一定間隔で姿勢を取り出す(最後は端数があっても終端ちょうどにする)
        for (int f = 0; f <= frameCount; ++f)
        {
            const float t = (f == frameCount) ? duration : (f * EditorConst::kFrameDuration);
            FbxTime fbxTime;
            fbxTime.SetSecondDouble(startSec + t);

            //AnimStackを切り替えたあとに古い計算結果が使われないよう、毎回計算させる(最後の引数true)
            const Transform pose = ToTransform(node->EvaluateLocalTransform(fbxTime, FbxNode::eSourcePivot, false, true));

            track->translationTrack_.AddKey(t, pose.translation_);
            track->rotationTrack_.AddKey(t, pose.rotation_);
            track->scaleTrack_.AddKey(t, pose.scale_);
        }
    }

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

        //アニメーションを含まない初期状態のローカル行列を、バインドポーズにする
        //(フレーム0だと、モデルにアニメーションが入っているときに、その最初の姿勢になってしまう)
        Transform localBindPose = ToTransform(node->EvaluateLocalTransform(FBXSDK_TIME_INFINITE));
        //自分のインデックスを取得し、子ノード探索時の親インデックスとして渡す
        currentBoneIndex = outSkeleton.AddBone(boneName, parentIndex, localBindPose);
    }
    //子ノードの深さ優先探索
    for (int i = 0; i < node->GetChildCount(); ++i)
    {
        ProcessSkeletonHierarchy(node->GetChild(i), currentBoneIndex, outSkeleton);
    }
}