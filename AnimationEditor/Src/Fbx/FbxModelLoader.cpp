#include "FbxModelLoader.h"
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <DxLib.h>

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

            std::string candidate1 = fbmDir + "/" + fileName;
            if (std::filesystem::exists(candidate1)) return candidate1;

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

            std::string candidate2 = fbxDir + "/" + fileName;
            if (std::filesystem::exists(candidate2)) return candidate2;

            if (std::filesystem::exists(fileTex->GetFileName()))
            {
                return fileTex->GetFileName();
            }

            return candidate1;
        }
    }
    return "";
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

/// @brief FBXファイルからボーン階層を構築する
bool FbxModelLoader::LoadSkeleton(const std::string& filePath, Skeleton& outSkeleton)
{
    if (!Initialize()) return false;

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

    // ※ ConvertScene は頂点・クラスター行列を変換せずボーンとズレるため呼ばない
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

/// @brief スキンメッシュとボーンウェイト、逆バインド行列を読み込む
bool FbxModelLoader::LoadMeshWithSkinning(const std::string& filepath, Skeleton& skeleton, FbxMeshModel& outMeshModel)
{
    if (!fbxScene_) return false;

    FbxGeometryConverter geomConverter(fbxManager_);
    geomConverter.Triangulate(fbxScene_, true);

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
    outMeshModel.indices_.clear();
    outMeshModel.subMeshes_.clear();

    std::unordered_map<std::string, int> loadedTextures;

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

                // FBXクラスターから正確なバインドワールド行列を取得し、その逆行列をセット
// ★ 修正: transformMatrix は掛けず、transformLinkMatrix の逆行列だけを使う
                FbxAMatrix transformLinkMatrix;
                cluster->GetTransformLinkMatrix(transformLinkMatrix);

                XMFLOAT4X4 bindMat = ToXMFLOAT4X4(transformLinkMatrix);
                XMMATRIX xBind = XMLoadFloat4x4(&bindMat);
                XMVECTOR det;
                XMMATRIX xInvBind = XMMatrixInverse(&det, xBind);
                XMStoreFloat4x4(&const_cast<Bone&>(skeleton.GetBone(boneIdx)).invBindMatrix_, xInvBind);
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
            auto it = loadedTextures.find(texPath);
            if (it != loadedTextures.end())
            {
                texHandle = it->second;
            }
            else
            {
                texHandle = LoadGraph(texPath.c_str());
                if (texHandle != -1) loadedTextures[texPath] = texHandle;
            }
        }

        // DrawPolygon3D（非インデックス描画）用に頂点配列の開始位置を記録
        SubMesh subMesh;
        subMesh.startIndex_ = static_cast<int>(outMeshModel.sourceVertices_.size());
        subMesh.textureHandle_ = texHandle;

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
                outMeshModel.indices_.push_back(0); // ダミー（DrawPolygon3Dでは頂点順で直接描画）
            }
        }

        subMesh.indexCount_ = static_cast<int>(polygonCount * 3);
        outMeshModel.subMeshes_.push_back(subMesh);
    }

    return !outMeshModel.sourceVertices_.empty();
}

/// @brief アニメーションを読み込む
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
    scene->SetCurrentAnimationStack(animStack);

    outClip.name_ = animStack->GetName();

    FbxTimeSpan timeSpan = animStack->GetLocalTimeSpan();
    FbxTime startTime = timeSpan.GetStart();
    float duration = static_cast<float>(timeSpan.GetDuration().GetSecondDouble());

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

    for (size_t b = 0; b < skeleton.GetBoneCount(); ++b)
    {
        const auto& bone = skeleton.GetBone(b);
        FbxNode* node = scene->FindNodeByName(bone.name_.c_str());
        if (node)
        {
            BoneTrack* track = outClip.GetOrCreateTrack(static_cast<int>(b));
            track->boneIndex_ = static_cast<int>(b);

            const float frameRate = 30.0f;
            int frameCount = static_cast<int>(duration * frameRate);
            float dt = 1.0f / frameRate;

            for (int f = 0; f <= frameCount; ++f)
            {
                float t = (f == frameCount) ? duration : (f * dt);
                FbxTime fbxTime;
                fbxTime.SetSecondDouble(startSec + t);

                // ルートボーン（parentIndex_ == -1）の場合は親ノード（Armature等）の回転・スケールも含めたグローバル姿勢を使う
                FbxAMatrix localMatrix = (bone.parentIndex_ == -1)
                    ? node->EvaluateGlobalTransform(fbxTime)
                    : node->EvaluateLocalTransform(fbxTime);

                FbxVector4 lT = localMatrix.GetT();
                FbxQuaternion lQ = localMatrix.GetQ();
                FbxVector4 lS = localMatrix.GetS();

                XMFLOAT3 pos = { static_cast<float>(lT[0]), static_cast<float>(lT[1]), static_cast<float>(lT[2]) };
                XMFLOAT4 rot = { static_cast<float>(lQ[0]), static_cast<float>(lQ[1]), static_cast<float>(lQ[2]), static_cast<float>(lQ[3]) };
                XMFLOAT3 scl = { static_cast<float>(lS[0]), static_cast<float>(lS[1]), static_cast<float>(lS[2]) };

                track->translationTrack_.AddKey(t, pos, InterpolationType::Linear, false);
                track->rotationTrack_.AddKey(t, rot, InterpolationType::Linear, false);
                track->scaleTrack_.AddKey(t, scl, InterpolationType::Linear, false);
            }
        }
    }

    scene->Destroy();
    return !outClip.boneTracks_.empty();
}

/// @brief シーンノードを深さ優先探索してスケルトン階層を登録する
void FbxModelLoader::ProcessSkeletonHierarchy(FbxNode* node, int parentIndex, Skeleton& outSkeleton)
{
    if (!node) return;

    FbxNodeAttribute* attr = node->GetNodeAttribute();
    int currentBoneIndex = parentIndex;

    if (attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
    {
        std::string boneName = node->GetName();

        // ルートボーン（parentIndex == -1）は親ノード（Armature等）の軸回転・スケールを含めたワールド姿勢を取得（上下反転防止）
        FbxAMatrix localMatrix = (parentIndex == -1)
            ? node->EvaluateGlobalTransform(0)
            : node->EvaluateLocalTransform(0);

        FbxVector4 lT = localMatrix.GetT();
        FbxQuaternion lQ = localMatrix.GetQ();
        FbxVector4 lS = localMatrix.GetS();

        XMFLOAT3 pos = { static_cast<float>(lT[0]), static_cast<float>(lT[1]), static_cast<float>(lT[2]) };
        XMFLOAT4 rot = { static_cast<float>(lQ[0]), static_cast<float>(lQ[1]), static_cast<float>(lQ[2]), static_cast<float>(lQ[3]) };
        XMFLOAT3 scl = { static_cast<float>(lS[0]), static_cast<float>(lS[1]), static_cast<float>(lS[2]) };

        Transform localBindPose(pos, rot, scl);
        currentBoneIndex = outSkeleton.AddBone(boneName, parentIndex, localBindPose);
    }

    for (int i = 0; i < node->GetChildCount(); ++i)
    {
        ProcessSkeletonHierarchy(node->GetChild(i), currentBoneIndex, outSkeleton);
    }
}