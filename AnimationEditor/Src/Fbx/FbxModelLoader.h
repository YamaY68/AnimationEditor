#pragma once
#include<string>
#include<vector>
#include<fbxsdk.h>

#if defined(_DEBUG)
#pragma comment(lib, "libfbxsdk-mt.lib")
#pragma comment(lib, "libxml2-mt.lib")
#pragma comment(lib, "zlib-mt.lib")
#else
#pragma comment(lib, "libfbxsdk-mt.lib")
#pragma comment(lib, "libxml2-mt.lib")
#pragma comment(lib, "zlib-mt.lib")
#endif

// Windows 標準ライブラリ
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "bcrypt.lib")

#include "../Animation/Skeleton.h"
#include "../Animation/AnimationClip.h"
#include "FbxMeshModel.h"

class FbxModelLoader
{
public:
	FbxModelLoader(void);
	~FbxModelLoader(void);

	bool Initialize(void);
	void Finalize(void);
	//FBXファイルからボーン階層を読み込む
	bool LoadSkeleton(const std::string& filePath, Skeleton& outSkeleton);
	//FBXファイルからスキンメッシュとボーンウェイトを読み込む
	bool LoadMeshWithSkinning(const std::string& filepath, const Skeleton& skeleton, FbxMeshModel& outMeshModel);
	//FBXからアニメーションクリップを読み込む
	bool LoadAnimation(const std::string& filePath, const Skeleton& skeleton, AnimationClip& outClip);

private:
	//シーンノードを深さ優先操作してスケルトン階層を構築
	void ProcessSkeletonHierarchy(FbxNode* node, int parentIndex, Skeleton& outSkeleton);
	/// <summary>
	/// 単一ノードのローカルTransformからアニメーションキーをサンプリング
	/// </summary>
	/// <param name="node">対象ノード</param>
	/// <param name="boneIndex">対象ボーンID</param>
	/// <param name="duration">クリップ全体の再生時間</param>
	/// <param name="outTrack">出力先ボーントラック</param>
	void ExtractNodeAnimation(FbxNode* node, int boneIndex, float duration, BoneTrack& outTrack);

	FbxManager* fbxManager_ = nullptr;
	FbxIOSettings* ioSetting_ = nullptr;
	FbxScene* fbxScene_ = nullptr;
};
