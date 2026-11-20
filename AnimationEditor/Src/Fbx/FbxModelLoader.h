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
	bool LoadMeshWithSkinning(const std::string& filepath, Skeleton& skeleton, FbxMeshModel& outMeshModel);

	//FBXファイルに入っているアニメーションを全部読み込む(AnimStack1つにつき1クリップ)
	//戻り値:読み込めたクリップの数
	int LoadAnimations(const std::string& filePath, const Skeleton& skeleton, std::vector<AnimationClip>& outClips);

	//LoadSkeletonで開いたモデルのFBXに入っているアニメーションを読み込む(ファイルは開き直さない)
	//戻り値:読み込めたクリップの数
	int LoadEmbeddedAnimations(const Skeleton& skeleton, std::vector<AnimationClip>& outClips);
private:
	//シーンノードを深さ優先操作してスケルトン階層を構築
	void ProcessSkeletonHierarchy(FbxNode* node, int parentIndex, Skeleton& outSkeleton);

	//FBXファイルを開き、新しいシーンに読み込む(失敗したらnullptr)
	FbxScene* ImportScene(const std::string& filePath, const char* sceneName);

	//シーンに入っている全AnimStackをクリップにする(戻り値:クリップの数)
	int SampleAllAnimStacks(FbxScene* scene, const Skeleton& skeleton, std::vector<AnimationClip>& outClips);

	//1つのAnimStackを一定間隔でサンプリングしてクリップにする
	bool SampleAnimStack(FbxScene* scene, FbxAnimStack* animStack, const Skeleton& skeleton, AnimationClip& outClip);

	FbxManager* fbxManager_ = nullptr;	//FBX　SDK全体のメモリ、オブジェクト工場
	FbxIOSettings* ioSetting_ = nullptr;	//読み込み時の設定
	FbxScene* fbxScene_ = nullptr;	//現在開いている１体分のFBXシーンデータ全体
};
