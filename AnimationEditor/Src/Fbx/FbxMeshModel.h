#pragma once
#include<vector>
#include<DirectXMath.h>
#include<DxLib.h>
#include"../Animation/Skeleton.h"

//１頂点分のスキニング情報
struct SkinVertex
{
	DirectX::XMFLOAT3 position_ = { 0.0f,0.0f,0.0f };	//頂点(元座標Tポーズ時)
	DirectX::XMFLOAT3 normal_ = { 0.0f,1.0f,0.0f };	//法線
	DirectX::XMFLOAT2 uv_ = { 0.0f,0.0f };	//UV座標
	float boneWeights_[4] = { 0.0f,0.0f,0.0f,0.0f };	//最大４ボーンのブレンド重み
	int boneIndices_[4] = { -1,-1,-1,-1 };	//影響を受けるボーンID
};
//パーツ、マテリアル単位の描画情報
struct SubMesh
{
	int startIndex_ = 0;	//インデックスバッファの開始位置
	int indexCount_ = 0;	//描画するインデックス数
	int textureHandle_ = -1;	//DxLibのテクスチャグラフィックハンドル
};

class FbxMeshModel
{
public:

	FbxMeshModel(void) = default;
	~FbxMeshModel(void);

	// コピー禁止（テクスチャハンドルの二重解放を防ぐ）
	FbxMeshModel(const FbxMeshModel&) = delete;
	FbxMeshModel& operator=(const FbxMeshModel&) = delete;

	// ムーブ（所有権を移す）
	FbxMeshModel(FbxMeshModel&& o) noexcept
		: sourceVertices_(std::move(o.sourceVertices_))
		, subMeshes_(std::move(o.subMeshes_))
		, skinnedVertices_(std::move(o.skinnedVertices_))
	{
		o.subMeshes_.clear();   
	}

	FbxMeshModel& operator=(FbxMeshModel&& o) noexcept
	{
		if (this != &o)
		{
			ReleaseTextures();  // 自分の古いテクスチャを解放
			sourceVertices_ = std::move(o.sourceVertices_);
			subMeshes_ = std::move(o.subMeshes_);
			skinnedVertices_ = std::move(o.skinnedVertices_);
			o.subMeshes_.clear();
		}
		return *this;
	}

	/// <summary>
	/// スケルトンのワールド行列に基づいて全頂点をスキニング変換し、描画バッファを更新
	/// </summary>
	/// <param name="skeleton">逆バインド行列を参照するスケルトン</param>
	/// <param name="worldMatrices">各ボーンのワールド行列配列</param>
	void UpdateSkinning(const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& worldMatrices);	//DxLibの３Ｄ描画関数を用いてスキニング済みメッシュを描画する
	void Draw(void)const;

	void ReleaseTextures(void);

	std::vector<SkinVertex>sourceVertices_;	//変形前の元頂点
	std::vector<SubMesh>subMeshes_;	//サブメッシュ
private:
	std::vector<VERTEX3D>skinnedVertices_;	//描画用の変換後頂点
};

