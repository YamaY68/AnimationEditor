#pragma once
#include<vector>
#include<string>
#include<DirectXMath.h>
#include<DxLib.h>
#include"../Animation/Skeleton.h"
using namespace DirectX;

//スキニングに必要な頂点情報
struct SkinVertex
{
	XMFLOAT3 position_ = { 0.0f,0.0f,0.0f };	//頂点
	XMFLOAT3 normal_ = { 0.0f,1.0f,0.0f };	//法線
	XMFLOAT2 uv_ = { 0.0f,0.0f };	//UV座標
	float boneWeights_[4] = { 0.0f,0.0f,0.0f,0.0f };	//最大４ボーンのブレンド重み
	int boneIndices_[4] = { -1,-1,-1,-1 };	//影響を受けるボーンID
};

struct SubMesh
{
	int startIndex_ = 0;	//インデックスバッファの開始位置
	int indexCount_ = 0;	//描画するインデックス数
	int textureHandle_ = -1;	//DxLibのテクスチャグラフィックハンドル
	std::string materialName_;	//マテリアル名
};

class FbxMeshModel
{
public:

	FbxMeshModel(void) = default;
	~FbxMeshModel(void);
	/// <summary>
	/// スケルトンのワールド行列に基づいて全頂点をスキニング変換し、描画バッファを更新
	/// </summary>
	/// <param name="skeleton">逆バインド行列を参照するスケルトン</param>
	/// <param name="worldMatrices">各ボーンのワールド行列配列</param>
	void UpdateSkinning(const Skeleton& skeleton, const std::vector<XMFLOAT4X4>& worldMatrices);
	//DxLibの３Ｄ描画関数を用いてスキニング済みメッシュを描画する
	void Draw(void)const;

	void ReleaseTextures(void);

	std::vector<SkinVertex>sourceVertices_;	//ソース頂点データ
	std::vector<unsigned short>indices_;	//ポリゴン構成インデックス
	std::vector<SubMesh>subMeshes_;	//サブメッシュ
private:
	std::vector<VERTEX3D>skinnedVertices_;	//スキニング計算結果を格納するDxLib描画用頂点バッファ
};

