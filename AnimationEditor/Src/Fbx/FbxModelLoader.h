#pragma once
#include<string>
#include<vector>
#include<fbxsdk.h>
#include<unordered_set>
#include<unordered_map>
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

	//モデルのFBXに入っているアニメーションの「名前と長さ」だけを読み込む(中身はまだ読まない)
	//中身はクリップが選ばれたときに、LoadEmbeddedAnimationで読み込む
	//戻り値:見つかったクリップの数
	int ListEmbeddedAnimations(std::vector<AnimationClip>& outClips);

	//ListEmbeddedAnimationsで作ったクリップの中身(元の動き)を読み込む
	//モデルを読み込み直したあとなど、読み込み元のシーンがもう無ければfalse
	bool LoadEmbeddedAnimation(const Skeleton& skeleton, AnimationClip& inOutClip); 
private:
	//ボーンとして扱うノードを集める
	//(スケルトン属性のノード＋スキンが参照しているノード＋それらの祖先)
	std::unordered_set<FbxNode*> CollectBoneNodes(FbxScene* scene)const;

	//シーンノードを深さ優先操作してスケルトン階層を構築
	void ProcessSkeletonHierarchy(FbxNode* node, int parentIndex,
		const std::unordered_set<FbxNode*>& boneNodes, Skeleton& outSkeleton);

	//ノードに一番近いボーン(自分自身か祖先)の番号を探す(無ければ-1)
	int FindNearestBone(FbxNode* node)const;
	//FBXファイルを開き、新しいシーンに読み込む(失敗したらnullptr)
	FbxScene* ImportScene(const std::string& filePath, const char* sceneName);

	//シーンに入っている全AnimStackをクリップにする(戻り値:クリップの数)
	int SampleAllAnimStacks(FbxScene* scene, const Skeleton& skeleton, std::vector<AnimationClip>& outClips);

	//1つのAnimStackを一定間隔でサンプリングしてクリップにする
	bool SampleAnimStack(FbxScene* scene, FbxAnimStack* animStack, const Skeleton& skeleton, AnimationClip& outClip);

	FbxManager* fbxManager_ = nullptr;	//FBX　SDK全体のメモリ、オブジェクト工場
	FbxIOSettings* ioSetting_ = nullptr;	//読み込み時の設定
	FbxScene* fbxScene_ = nullptr;	//現在開いている１体分のFBXシーンデータ全体
	int sceneId_ = 0;	//fbxScene_を入れ替えるたびに増やす番号(遅延読み込みのクリップが、どのシーンのものか確かめるため)
	std::unordered_map<FbxNode*, int> nodeToBone_;	//ノード→ボーン番号(LoadSkeletonで作り、メッシュの読み込みで使う)
};
