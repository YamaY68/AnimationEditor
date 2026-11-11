#include "FbxModelLoader.h"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <functional>
#include <DxLib.h>

using namespace DirectX;

// テクスチャファイルパス解決ヘルパー
static std::string ResolveFilePath(FbxNode* node, const std::string& fbxFilePath)
{
    if (!node) return "";

    std::filesystem::path fbxPath(fbxFilePath);
    std::string fbxDir = fbxPath.parent_path().string();
    std::string stemName = fbxPath.stem().string();
    std::string fbmDir = fbxDir + "/" + stemName + ".fbm";

    int matCount = node->GetMaterialCount();
    for (int m = 0; m < matCount; ++m)
    {
        FbxSurfaceMaterial* mat = node->GetMaterial(m);
        if (!mat) continue;

        FbxProperty prop = mat->FindProperty(FbxSurfaceMaterial::sDiffuse);
        if (!prop.IsValid()) continue;

        FbxFileTexture* fileTex = nullptr;
        int layeredCount = prop.GetSrcObjectCount<FbxLayeredTexture>();
        if (layeredCount > 0)
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

        if (fileTex)
        {
            std::string rawName = fileTex->GetRelativeFileName();
            if (rawName.empty()) rawName = fileTex->GetFileName();
            std::string fileName = std::filesystem::path(rawName).filename().string();

            // 1. <FBX名>.fbm フォルダ探索
            std::string candidate1 = fbmDir + "/" + fileName;
            if (std::filesystem::exists(candidate1)) return candidate1;

            // 2. ディレクトリ内の別名 .fbm フォルダ探索
            if (std::filesystem::exists(fbxDir))
            {
                for (const auto& entry : std::filesystem::directory_iterator(fbxDir))
                {
                    if (entry.is_directory() && entry.path().extension() == ".fbm")
                    {
                        std::string candidateFbm = entry.path().string() + "/" + fileName;
                        if (std::filesystem::exists(candidateFbm)) return candidateFbm;
                    }
                }
            }

            // 3. FBXファイルと同階層
            std::string candidate2 = fbxDir + "/" + fileName;
            if (std::filesystem::exists(candidate2)) return candidate2;

            // 4. フルパス
            if (std::filesystem::exists(fileTex->GetFileName()))
            {
                return fileTex->GetFileName();
            }

            return candidate1;
        }
    }
    return "";
}

// コンストラクタ
FbxModelLoader::FbxModelLoader(void)
{
}

// デストラクタ
FbxModelLoader::~FbxModelLoader(void)
{
    Finalize();
}

// 初期化
bool FbxModelLoader::Initialize(void)
{
    if (!fbxManager_)
    {
        fbxManager_ = FbxManager::Create();
        if (!fbxManager_) return false;

        ioSetting_ = FbxIOSettings::Create(fbxManager_, IOSROOT);
        fbxManager_->SetIOSettings(ioSetting_);
    }
    return true;
}

// 終了処理
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

/// @brief FBXファイルからボーン階層を構築する
/// @param filePath ファイルパス
/// @param outSkeleton 出力先スケルトン
/// @return 成功時 true
bool FbxModelLoader::LoadSkeleton(const std::string& filePath, Skeleton& outSkeleton)
{
    if (!Initialize()) return false;

    // Autodesk SDK側のインポーターであることを明示
    fbxsdk::FbxImporter* sdkImporter = fbxsdk::FbxImporter::Create(fbxManager_, "");
    if (!sdkImporter->Initialize(filePath.c_str(), -1, ioSetting_))
    {
        sdkImporter->Destroy();
        return false;
    }

    if (fbxScene_) fbxScene_->Destroy();
    fbxScene_ = FbxScene::Create(fbxManager_, "ImportScene");
    sdkImporter->Import(fbxScene_);
    sdkImporter->Destroy();

    // 座標系を DirectX（左手系・Y-up）に変換
    FbxAxisSystem directXAxis = FbxAxisSystem::DirectX;
    directXAxis.ConvertScene(fbxScene_);

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

/// @brief スキンメッシュとボーンウェイトを読み込む
/// @param filepath ファイルパス
/// @param skeleton 対応スケルトン
/// @param outMeshModel 出力先メッシュモデル
/// @return 成功時 true
bool FbxModelLoader::LoadMeshWithSkinning(const std::string& filepath, const Skeleton& skeleton, FbxMeshModel& outMeshModel)
{
    if (!fbxScene_) return false;

    std::vector<FbxNode*> meshNodes;
    std::function<void(FbxNode*)> findMeshes = [&](FbxNode* n)
        {
            if (!n) return;
            if (n->GetMesh()) meshNodes.push_back(n);
            for (int i = 0; i < n->GetChildCount(); ++i) findMeshes(n->GetChild(i));
        };
    findMeshes(fbxScene_->GetRootNode());

    if (meshNodes.empty()) return false;

    outMeshModel.sourceVertices_.clear();
    outMeshModel.indices_.clear();
    outMeshModel.subMeshes_.clear();

    for (FbxNode* node : meshNodes)
    {
        FbxMesh* mesh = node->GetMesh();
        if (!mesh) continue;

        int cpCount = mesh->GetControlPointsCount();
        FbxVector4* controlPoints = mesh->GetControlPoints();

        struct VertexWeightInfo
        {
            int indices[4] = { -1, -1, -1, -1 };
            float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        };
        std::vector<VertexWeightInfo> cpWeights(cpCount);

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

                int indexCount = cluster->GetControlPointIndicesCount();
                int* cpIndices = cluster->GetControlPointIndices();
                double* weights = cluster->GetControlPointWeights();

                for (int k = 0; k < indexCount; ++k)
                {
                    int cpIdx = cpIndices[k];
                    float w = static_cast<float>(weights[k]);
                    if (cpIdx < 0 || cpIdx >= cpCount) continue;

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

        const char* uvSetName = nullptr;
        if (mesh->GetElementUVCount() > 0)
        {
            uvSetName = mesh->GetElementUV(0)->GetName();
        }

        std::string texPath = ResolveFilePath(node, filepath);
        int texHandle = -1;
        if (!texPath.empty())
        {
            texHandle = LoadGraph(texPath.c_str());
        }

        SubMesh subMesh;
        subMesh.startIndex_ = static_cast<unsigned int>(outMeshModel.sourceVertices_.size());
        subMesh.textureHandle_ = texHandle;

        int polygonCount = mesh->GetPolygonCount();
        for (int p = 0; p < polygonCount; ++p)
        {
            unsigned short currentStartIdx = static_cast<unsigned short>(outMeshModel.sourceVertices_.size());

            for (int v = 0; v < 3; ++v)
            {
                int cpIndex = mesh->GetPolygonVertex(p, v);

                SkinVertex vert;
                FbxVector4 pos = controlPoints[cpIndex];
                vert.position_ = { static_cast<float>(pos[0]), static_cast<float>(pos[1]), static_cast<float>(pos[2]) };

                FbxVector4 norm;
                if (mesh->GetPolygonVertexNormal(p, v, norm))
                {
                    vert.normal_ = { static_cast<float>(norm[0]), static_cast<float>(norm[1]), static_cast<float>(norm[2]) };
                }

                if (uvSetName)
                {
                    FbxVector2 uv;
                    bool unmapped;
                    if (mesh->GetPolygonVertexUV(p, v, uvSetName, uv, unmapped))
                    {
                        vert.uv_ = { static_cast<float>(uv[0]), 1.0f - static_cast<float>(uv[1]) };
                    }
                }

                for (int k = 0; k < 4; ++k)
                {
                    vert.boneWeights_[k] = cpWeights[cpIndex].weights[k];
                    vert.boneIndices_[k] = cpWeights[cpIndex].indices[k];
                }

                float totalWeight = vert.boneWeights_[0] + vert.boneWeights_[1] + vert.boneWeights_[2] + vert.boneWeights_[3];
                if (totalWeight > 0.0001f)
                {
                    for (int k = 0; k < 4; ++k) vert.boneWeights_[k] /= totalWeight;
                }

                outMeshModel.sourceVertices_.push_back(vert);
            }

            outMeshModel.indices_.push_back(currentStartIdx + 0);
            outMeshModel.indices_.push_back(currentStartIdx + 2);
            outMeshModel.indices_.push_back(currentStartIdx + 1);
        }

        subMesh.indexCount_ = static_cast<unsigned int>(polygonCount * 3);
        outMeshModel.subMeshes_.push_back(subMesh);
    }

    return !outMeshModel.sourceVertices_.empty();
}

/// @brief アニメーションを読み込む（0秒と終了秒の2点サンプリング）
/// @param filePath ファイルパス
/// @param skeleton 対応スケルトン
/// @param outClip 出力先アニメーションクリップ
/// @return 成功時 true
bool FbxModelLoader::LoadAnimation(const std::string& filePath, const Skeleton& skeleton, AnimationClip& outClip)
{
    if (!Initialize()) return false;

    fbxsdk::FbxImporter* sdkImporter = fbxsdk::FbxImporter::Create(fbxManager_, "");
    if (!sdkImporter->Initialize(filePath.c_str(), -1, ioSetting_))
    {
        sdkImporter->Destroy();
        return false;
    }

    FbxScene* scene = FbxScene::Create(fbxManager_, "AnimScene");
    sdkImporter->Import(scene);
    sdkImporter->Destroy();

    int animStackCount = scene->GetSrcObjectCount<FbxAnimStack>();
    if (animStackCount == 0)
    {
        scene->Destroy();
        return false;
    }

    FbxAnimStack* animStack = scene->GetSrcObject<FbxAnimStack>(0);
    outClip.name_ = animStack->GetName();

    FbxTimeSpan timeSpan = animStack->GetLocalTimeSpan();
    float duration = static_cast<float>(timeSpan.GetDuration().GetSecondDouble());

    if (duration <= 0.05f)
    {
        FbxTakeInfo* takeInfo = scene->GetTakeInfo(animStack->GetName());
        if (takeInfo)
        {
            duration = static_cast<float>(takeInfo->mLocalTimeSpan.GetDuration().GetSecondDouble());
        }
        else
        {
            scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(timeSpan);
            duration = static_cast<float>(timeSpan.GetDuration().GetSecondDouble());
        }
    }

    outClip.duration_ = duration;
    outClip.boneTracks_.clear();

    for (size_t b = 0; b < skeleton.GetBoneCount(); ++b)
    {
        const auto& bone = skeleton.GetBone(b);
        FbxNode* node = scene->FindNodeByName(bone.name_.c_str());
        if (node)
        {
            BoneTrack* track = outClip.GetOrCreateTrack(static_cast<int>(b));
            ExtractNodeAnimation(node, static_cast<int>(b), duration, *track);
        }
    }

    scene->Destroy();
    return !outClip.boneTracks_.empty();
}

/// @brief シーンノードを深さ優先探索してスケルトン階層を登録する
/// @param node 走査対象ノード
/// @param parentIndex 親ボーンインデックス
/// @param outSkeleton 出力先スケルトン
void FbxModelLoader::ProcessSkeletonHierarchy(FbxNode* node, int parentIndex, Skeleton& outSkeleton)
{
    if (!node) return;

    FbxNodeAttribute* attr = node->GetNodeAttribute();
    int currentBoneIndex = parentIndex;

    if (attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
    {
        std::string boneName = node->GetName();

        FbxDouble3 lT = node->LclTranslation.Get();
        FbxDouble3 lR = node->LclRotation.Get();
        FbxDouble3 lS = node->LclScaling.Get();

        XMFLOAT3 pos = { static_cast<float>(lT[0]), static_cast<float>(lT[1]), static_cast<float>(lT[2]) };
        XMFLOAT3 scl = { static_cast<float>(lS[0]), static_cast<float>(lS[1]), static_cast<float>(lS[2]) };

        float pitch = XMConvertToRadians(static_cast<float>(lR[0]));
        float yaw = XMConvertToRadians(static_cast<float>(lR[1]));
        float roll = XMConvertToRadians(static_cast<float>(lR[2]));

        XMVECTOR q = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
        XMFLOAT4 rot;
        XMStoreFloat4(&rot, q);

        Transform localBindPose(pos, rot, scl);
        currentBoneIndex = outSkeleton.AddBone(boneName, parentIndex, localBindPose);
    }

    for (int i = 0; i < node->GetChildCount(); ++i)
    {
        ProcessSkeletonHierarchy(node->GetChild(i), currentBoneIndex, outSkeleton);
    }
}

/// @brief 単一ノードのアニメーションを0秒と終了秒の2点のみサンプリングする
/// @param node 対象ノード
/// @param boneIndex 対象ボーンID
/// @param duration 再生時間（秒）
/// @param outTrack 出力先ボーントラック
void FbxModelLoader::ExtractNodeAnimation(FbxNode* node, int boneIndex, float duration, BoneTrack& outTrack)
{
    outTrack.boneIndex_ = boneIndex;

    float sampleTimes[2] = { 0.0f, duration };
    for (float t : sampleTimes)
    {
        FbxTime fbxTime;
        fbxTime.SetSecondDouble(t);

        FbxAMatrix localMatrix = node->EvaluateLocalTransform(fbxTime);

        FbxVector4 lT = localMatrix.GetT();
        FbxQuaternion lQ = localMatrix.GetQ();
        FbxVector4 lS = localMatrix.GetS();

        XMFLOAT3 pos = { static_cast<float>(lT[0]), static_cast<float>(lT[1]), static_cast<float>(lT[2]) };
        XMFLOAT4 rot = { static_cast<float>(lQ[0]), static_cast<float>(lQ[1]), static_cast<float>(lQ[2]), static_cast<float>(lQ[3]) };
        XMFLOAT3 scl = { static_cast<float>(lS[0]), static_cast<float>(lS[1]), static_cast<float>(lS[2]) };

        outTrack.translationTrack_.AddKey(t, pos, InterpolationType::Linear);
        outTrack.rotationTrack_.AddKey(t, rot, InterpolationType::Linear);
        outTrack.scaleTrack_.AddKey(t, scl, InterpolationType::Linear);
    }
}