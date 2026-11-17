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
	bool LoadMeshWithSkinning(const std::string& filepath, Skeleton& skeleton, FbxMeshModel& outMeshModel);	//FBXからアニメーションクリップを読み込む
	bool LoadAnimation(const std::string& filePath, const Skeleton& skeleton, AnimationClip& outClip);

private:
	//シーンノードを深さ優先操作してスケルトン階層を構築
	void ProcessSkeletonHierarchy(FbxNode* node, int parentIndex, Skeleton& outSkeleton);

	FbxManager* fbxManager_ = nullptr;	//FBX　SDK全体のメモリ、オブジェクト工場
	FbxIOSettings* ioSetting_ = nullptr;	//読み込み時の設定
	FbxScene* fbxScene_ = nullptr;	//現在開いている１体分のFBXシーンデータ全体
};
