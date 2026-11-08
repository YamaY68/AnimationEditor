#include "FbxImporter.h"
#include <filesystem>

#include <filesystem>

// ノードのマテリアルからテクスチャファイルパスを抽出・解決する
static std::string ExtractTextureFilePath(FbxNode* node, const std::string& fbxFilePath)
{
    if (!node) return "";

    std::filesystem::path fbxPath(fbxFilePath);
    std::string fbxDir = fbxPath.parent_path().string();              // 例: "Data"
    std::string stemName = fbxPath.stem().string();                   // 例: "TPose" や "test"
    std::string fbmDir = fbxDir + "/" + stemName + ".fbm";             // 例: "Data/TPose.fbm" または "Data/test.fbm"

    int materialCount = node->GetMaterialCount();
    for (int m = 0; m < materialCount; ++m)
    {
        FbxSurfaceMaterial* material = node->GetMaterial(m);
        if (!material) continue;

        FbxProperty prop = material->FindProperty(FbxSurfaceMaterial::sDiffuse);
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
            // FBXに記録されている元のファイル名（例: "Paladin_diffuse.png"）
            std::string rawName = fileTex->GetRelativeFileName();
            if (rawName.empty()) rawName = fileTex->GetFileName();
            std::string filename = std::filesystem::path(rawName).filename().string();

            // 1. "<FBX名>.fbm/ファイル名" を探索（例: Data/TPose.fbm/Paladin_diffuse.png）
            std::string candidate1 = fbmDir + "/" + filename;
            if (std::filesystem::exists(candidate1)) return candidate1;

            // 2. モーション名や別名で展開された ".fbm" フォルダ内を探索（例: Data/test.fbm/Paladin_diffuse.png）
            if (std::filesystem::exists(fbxDir))
            {
                for (const auto& entry : std::filesystem::directory_iterator(fbxDir))
                {
                    if (entry.is_directory() && entry.path().extension() == ".fbm")
                    {
                        std::string candidateFbm = entry.path().string() + "/" + filename;
                        if (std::filesystem::exists(candidateFbm)) return candidateFbm;
                    }
                }
            }

            // 3. FBXと同じ階層直下を探索（例: Data/Paladin_diffuse.png）
            std::string candidate2 = fbxDir + "/" + filename;
            if (std::filesystem::exists(candidate2)) return candidate2;

            // 4. FBX内部に記録されていたフルパスを探索
            if (std::filesystem::exists(fileTex->GetFileName()))
            {
                return fileTex->GetFileName();
            }

            // 見つからなかった場合のフォールバック（一番確率の高い候補を返す）
            return candidate1;
        }
    }
    return "";
}

namespace Editor
{


Editor::FbxImporter::FbxImporter(void)
{
}

Editor::FbxImporter::~FbxImporter(void)
{
	Release();
}

}

bool Editor::FbxImporter::Init(void)
{
	if (!fbxManager_)
	{
		fbxManager_ = FbxManager::Create();
		if (!fbxManager_)return false;
	}

	ioSettings_ = FbxIOSettings::Create(fbxManager_, IOSROOT);
	fbxManager_->SetIOSettings(ioSettings_);
	return true;
}

void Editor::FbxImporter::Release(void)
{
	if (ioSettings_) {
		ioSettings_->Destroy();
		ioSettings_ = nullptr;
	}
	if (fbxManager_) {
		fbxManager_->Destroy();
		fbxManager_ = nullptr;
	}
}

bool Editor::FbxImporter::LoadSkeleton(const std::string& filePath, Skeleton& outSkeleton)
{
	if (!Init())return false;

	fbxsdk::FbxImporter* importer = fbxsdk::FbxImporter::Create(fbxManager_, "");
	if (!importer->Initialize(filePath.c_str(), -1, ioSettings_))
	{
		importer->Destroy();
		return false;
	}

	FbxScene* scene = FbxScene::Create(fbxManager_, "ImportScene");
	importer->Import(scene);
	importer->Destroy();

	FbxNode* rootNode = scene->GetRootNode();
	if (rootNode)
	{
		int childCount = rootNode->GetChildCount();
		for (int i = 0; i < childCount; ++i)
		{
			ProcessSkeletonHierarchy(rootNode->GetChild(i), -1, outSkeleton);
		}
	}
	scene->Destroy();
	return outSkeleton.GetBoneCount() > 0;
}

bool Editor::FbxImporter::LoadAnimation(const std::string& filePath, const Skeleton& skeleton, AnimationClip& outClip)
{
	if (!Init())return false;

	fbxsdk::FbxImporter* importer = fbxsdk::FbxImporter::Create(fbxManager_, "");
	if (!importer->Initialize(filePath.c_str(), -1, ioSettings_))
	{
		importer->Destroy();
		return false;
	}

	FbxScene* scene = FbxScene::Create(fbxManager_, "AnimScene");
	importer->Import(scene);
	importer->Destroy();

	int animStackCount = scene->GetSrcObjectCount<FbxAnimStack>();
	if (animStackCount == 0)
	{
		scene->Destroy();
		return false;
	}

	FbxAnimStack* animStack = scene->GetSrcObject<FbxAnimStack>(0);
	FbxTakeInfo* takeInfo = scene->GetTakeInfo(animStack->GetName());

	float duration = 0.0f;
	if (takeInfo)
	{
		FbxTime importDuration = takeInfo->mLocalTimeSpan.GetDuration();
		duration = static_cast<float>(importDuration.GetSecondDouble());
	}
	else
	{
		FbxTimeSpan timeSpan;
		scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(timeSpan);
		duration = static_cast<float>(timeSpan.GetDuration().GetSecondDouble());
	}

	outClip.name_ = animStack->GetName();
	outClip.duration_ = duration;

	FbxAnimLayer* animLayer = animStack->GetMember<FbxAnimLayer>(0);
	if (!animLayer)
	{
		scene->Destroy();
		return false;
	}

	const float  frameRate = 60.0f;
	outClip.boneTracks_.reserve(skeleton.GetBoneCount());

	for (int i = 0; i < skeleton.GetBoneCount(); ++i)
	{
		const auto& bone = skeleton.GetBone(i);
		FbxNode* node = scene->FindNodeByName(bone.name_.c_str());
		if (node)
		{
			BoneTrack* track = outClip.GetOrCreateTrack(i);
			ExtractNodeAnimation(node, i, animLayer, duration, frameRate, *track);
		}
	}
	scene->Destroy();
	return !outClip.boneTracks_.empty();
}

bool Editor::FbxImporter::LoadMeshWithSkinning(const std::string& filePath, Skeleton& skeleton, FbxMeshModel& outModel)
{
    if (!Init()) return false;

    fbxsdk::FbxImporter* importer = fbxsdk::FbxImporter::Create(fbxManager_, "");
    if (!importer->Initialize(filePath.c_str(), -1, ioSettings_))
    {
        importer->Destroy();
        return false;
    }

    FbxScene* scene = FbxScene::Create(fbxManager_, "MeshScene");
    importer->Import(scene);
    importer->Destroy();

    // ポリゴンをすべて三角形に分割（三角面化）
    FbxGeometryConverter geomConverter(fbxManager_);
    geomConverter.Triangulate(scene, true);

    FbxNode* rootNode = scene->GetRootNode();
    if (!rootNode)
    {
        scene->Destroy();
        return false;
    }

    // メッシュノードを収集
    std::vector<FbxNode*> meshNodes;
    auto CollectMeshNodes = [&](auto self, FbxNode* node) -> void {
        if (!node) return;
        FbxNodeAttribute* attr = node->GetNodeAttribute();
        if (attr && attr->GetAttributeType() == FbxNodeAttribute::eMesh)
        {
            meshNodes.push_back(node);
        }
        for (int i = 0; i < node->GetChildCount(); ++i)
        {
            self(self, node->GetChild(i));
        }
        };
    CollectMeshNodes(CollectMeshNodes, rootNode);

    if (meshNodes.empty())
    {
        scene->Destroy();
        return false;
    }

    outModel.ReleaseTextures();
    outModel.sourceVertices_.clear();
    outModel.indices_.clear();
    outModel.subMeshes_.clear();

    // FBXの親ディレクトリ（同じ階層のテクスチャを検索するため）
    std::string fbxDir = std::filesystem::path(filePath).parent_path().string();

    std::unordered_map<std::string, int> loadedTextures;

    for (FbxNode* node : meshNodes)
    {
        FbxMesh* mesh = node->GetMesh();
        if (!mesh) continue;

        int controlPointsCount = mesh->GetControlPointsCount();
        FbxVector4* controlPoints = mesh->GetControlPoints();

        struct TempWeight
        {
            float weights[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
            int indices[4] = { 0, 0, 0, 0 };
            int count = 0;
            void Add(int bIdx, float w)
            {
                if (count < 4)
                {
                    indices[count] = bIdx;
                    weights[count] = w;
                    ++count;
                }
            }
        };
        std::vector<TempWeight> cpWeights(controlPointsCount);

        // スキンクラスターからバインド行列とウェイト抽出
        int deformerCount = mesh->GetDeformerCount(FbxDeformer::eSkin);
        for (int d = 0; d < deformerCount; ++d)
        {
            FbxSkin* skin = static_cast<FbxSkin*>(mesh->GetDeformer(d, FbxDeformer::eSkin));
            int clusterCount = skin->GetClusterCount();

            for (int c = 0; c < clusterCount; ++c)
            {
                FbxCluster* cluster = skin->GetCluster(c);
                FbxNode* linkNode = cluster->GetLink();
                if (!linkNode) continue;

                int boneIndex = skeleton.FindBoneIndex(linkNode->GetName());
                if (boneIndex == -1) continue;

                FbxAMatrix transformLinkMatrix;
                cluster->GetTransformLinkMatrix(transformLinkMatrix);

                using namespace DirectX;
                XMFLOAT4X4 bindMat;
                for (int r = 0; r < 4; ++r) {
                    for (int col = 0; col < 4; ++col) {
                        bindMat.m[r][col] = static_cast<float>(transformLinkMatrix.Get(r, col));
                    }
                }
                XMMATRIX xBind = XMLoadFloat4x4(&bindMat);
                XMVECTOR det;
                XMMATRIX xInvBind = XMMatrixInverse(&det, xBind);
                XMStoreFloat4x4(&skeleton.GetBone(boneIndex).invBindMatrix_, xInvBind);

                int cpIndexCount = cluster->GetControlPointIndicesCount();
                int* cpIndices = cluster->GetControlPointIndices();
                double* weights = cluster->GetControlPointWeights();

                for (int k = 0; k < cpIndexCount; ++k)
                {
                    int cpIdx = cpIndices[k];
                    if (cpIdx < controlPointsCount)
                    {
                        cpWeights[cpIdx].Add(boneIndex, static_cast<float>(weights[k]));
                    }
                }
            }
        }

        // テクスチャのロード
        std::string texPath = ExtractTextureFilePath(node, filePath);
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
                if (texHandle != -1)
                {
                    loadedTextures[texPath] = texHandle;
                }
            }
        }

        SubMesh subMesh;
        subMesh.indexOffset_ = static_cast<unsigned int>(outModel.indices_.size());
        subMesh.textureHandle_ = texHandle;

        FbxStringList uvNames;
        mesh->GetUVSetNames(uvNames);
        const char* uvSetName = uvNames.GetCount() > 0 ? uvNames.GetStringAt(0) : nullptr;

        int polygonCount = mesh->GetPolygonCount();

        for (int p = 0; p < polygonCount; ++p)
        {
            // 現在の頂点配列の末尾インデックス（これから追加する3頂点の先頭）
            unsigned short currentStartIdx = static_cast<unsigned short>(outModel.sourceVertices_.size());

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
                    for (int k = 0; k < 4; ++k)
                    {
                        vert.boneWeights_[k] /= totalWeight;
                    }
                }

                outModel.sourceVertices_.push_back(vert);
            }

            // 追加した3頂点に対して時計回りでインデックス登録
            outModel.indices_.push_back(currentStartIdx + 0);
            outModel.indices_.push_back(currentStartIdx + 2);
            outModel.indices_.push_back(currentStartIdx + 1);
        }

        subMesh.indexCount_ = static_cast<unsigned int>(polygonCount * 3);
        outModel.subMeshes_.push_back(subMesh);
    }

    outModel.PrepareBuffer();
    scene->Destroy();
    return !outModel.sourceVertices_.empty();
}

void Editor::FbxImporter::ProcessSkeletonHierarchy(FbxNode* node, int parentIndex, Skeleton& outSkeleton)
{
	if (!node)return;

	int currentIndex = parentIndex;

	FbxNodeAttribute* attr = node->GetNodeAttribute();
	if (attr && attr->GetAttributeType() == FbxNodeAttribute::eSkeleton)
	{
		Transform localBind = ExtractLocalTransform(node);
		currentIndex = outSkeleton.AddBone(node->GetName(), parentIndex, localBind);
	}

	int childCount = node->GetChildCount();
	for (int i = 0; i < childCount; ++i)
	{
		ProcessSkeletonHierarchy(node->GetChild(i), currentIndex, outSkeleton);
	}
}

Transform Editor::FbxImporter::ExtractLocalTransform(FbxNode* node)
{
	Transform transform;
	if (!node)return transform;

	FbxDouble3 lT = node->LclTranslation.Get();
	transform.translation_ = { static_cast<float>(lT[0]),static_cast<float>(lT[1]),static_cast<float>(lT[2]) };

	FbxDouble3 lR = node->LclRotation.Get();
	using namespace DirectX;
	float pitch = XMConvertToRadians(static_cast<float>(lR[0]));
	float yaw = XMConvertToRadians(static_cast<float>(lR[1]));
	float roll = XMConvertToRadians(static_cast<float>(lR[2]));
	XMVECTOR qRot = XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
	XMStoreFloat4(&transform.rotation_, qRot);

	FbxDouble3 lS = node->LclScaling.Get();
	transform.scale_ = { static_cast<float>(lS[0]),static_cast<float>(lS[1]),static_cast<float>(lS[2]) };
	return transform;
}

void Editor::FbxImporter::ExtractNodeAnimation(FbxNode* node, int boneIndex, FbxAnimLayer* animLayer, float duration, float frameRate, BoneTrack& outTrack)
{
    outTrack.boneIndex_ = boneIndex;

    int frameCount = static_cast<int>(duration * frameRate);
    float dt = 1.0f / frameRate;

    for (int f = 0; f <= frameCount; ++f)
    {
        float t = f * dt;
        FbxTime fbxTime;
        fbxTime.SetSecondDouble(t);

        FbxAMatrix localMatrix = node->EvaluateLocalTransform(fbxTime);

        FbxVector4 lT = localMatrix.GetT();
        FbxQuaternion lQ = localMatrix.GetQ();
        FbxVector4 lS = localMatrix.GetS();

        DirectX::XMFLOAT3 pos = { static_cast<float>(lT[0]), static_cast<float>(lT[1]), static_cast<float>(lT[2]) };
        DirectX::XMFLOAT4 rot = { static_cast<float>(lQ[0]), static_cast<float>(lQ[1]), static_cast<float>(lQ[2]), static_cast<float>(lQ[3]) };
        DirectX::XMFLOAT3 scl = { static_cast<float>(lS[0]), static_cast<float>(lS[1]), static_cast<float>(lS[2]) };

        // ★ 初期インポート時は isUserEdited_ = false で登録
        outTrack.translationTrack_.AddKey(t, pos, InterpolationType::Linear);
        outTrack.rotationTrack_.AddKey(t, rot, InterpolationType::Linear);
        outTrack.scaleTrack_.AddKey(t, scl, InterpolationType::Linear);
    }
}
