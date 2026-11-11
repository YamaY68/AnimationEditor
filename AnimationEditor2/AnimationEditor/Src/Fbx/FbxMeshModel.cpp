#include "FbxMeshModel.h"

FbxMeshModel::~FbxMeshModel(void)
{
	ReleaseTextures();
}

void FbxMeshModel::UpdateSkinning(const Skeleton& skeleton, const std::vector<XMFLOAT4X4>& worldMatrices)
{
	const size_t vertCount = sourceVertices_.size();
	if (vertCount == 0)return;

	if (skinnedVertices_.size() != vertCount)
	{
		skinnedVertices_.resize(vertCount);
	}

	const size_t boneCount = skeleton.GetBoneCount();

	//各ボーンの最終変形行列
	std::vector<XMMATRIX>skinMatrices(boneCount);
	for (size_t b = 0; b < boneCount; ++b)
	{
		if (b < worldMatrices.size())
		{
			XMMATRIX invBind = XMLoadFloat4x4(&skeleton.GetBone(b).invBindMatrix_);
			XMMATRIX currentWorld = XMLoadFloat4x4(&worldMatrices[b]);
			skinMatrices[b] = XMMatrixMultiply(invBind, currentWorld);
		}
		else
		{
			skinMatrices[b] = XMMatrixIdentity();
		}
	}
	
	//全頂点の座標と法線をスキニング
	for (size_t i = 0; i < vertCount; ++i)
	{
		const auto& src = sourceVertices_[i];

		XMVECTOR srcPos = XMLoadFloat3(&src.position_);
		XMVECTOR srcNorm = XMLoadFloat3(&src.normal_);

		XMVECTOR skinnedPos = XMVectorZero();
		XMVECTOR skinnedNorm = XMVectorZero();

		//最大４ボーン分の影響度をブレンド
		for (int k = 0; k < 4; ++k)
		{
			float weight = src.boneWeights_[k];
			if (weight <= 0.0001f)continue;

			int bIdx = src.boneIndices_[k];
			if (bIdx >= 0 && bIdx < static_cast<int>(boneCount))
			{
				XMVECTOR transformedPos = XMVector3TransformCoord(srcPos, skinMatrices[bIdx]);
				XMVECTOR transformNorm = XMVector3TransformNormal(srcNorm, skinMatrices[bIdx]);

				skinnedPos = XMVectorAdd(skinnedPos, XMVectorScale(transformedPos, weight));
				skinnedNorm = XMVectorAdd(skinnedNorm, XMVectorScale(transformNorm, weight));
			}
		}

		//DxLibの描画頂点バッファへ書き込み
		XMFLOAT3 outPos, outNorm;
		XMStoreFloat3(&outPos, skinnedPos);
		XMStoreFloat3(&outNorm, XMVector3Normalize(skinnedNorm));

		skinnedVertices_[i].pos = VGet(outPos.x, outPos.y, outPos.z);
		skinnedVertices_[i].norm = VGet(outNorm.x, outNorm.y, outNorm.z);
		skinnedVertices_[i].dif = GetColorU8(255,255,255,255);
		skinnedVertices_[i].spc = GetColorU8(0, 0, 0,0);
		skinnedVertices_[i].u = src.uv_.x;
		skinnedVertices_[i].v = src.uv_.y;
		skinnedVertices_[i].su = 0.0f;
		skinnedVertices_[i].sv = 0.0f;
	}
}

void FbxMeshModel::Draw(void) const
{
	if (skinnedVertices_.empty() || indices_.empty())return;

	//サブメッシュ単位で描画
	for (const auto& sub : subMeshes_)
	{
		if (sub.indexCount_ == 0)continue;

		int polygonCount = static_cast<int>(sub.indexCount_ / 3);
		const unsigned short* indexPtr = &indices_[sub.startIndex_];

		int tex = (sub.textureHandle_ != -1) ? sub.textureHandle_ : DX_NONE_GRAPH;

		DrawPolygonIndexed3D(
			const_cast<VERTEX3D*>(skinnedVertices_.data()),
			static_cast<int>(skinnedVertices_.size()),
			const_cast<unsigned short*>(indexPtr),
			polygonCount,
			tex,
			true
		);
	}
}

void FbxMeshModel::ReleaseTextures(void)
{
	for (auto& sub : subMeshes_)
	{
		if (sub.textureHandle_ != -1)
		{
			DeleteGraph(sub.textureHandle_);
			sub.textureHandle_ = -1;
		}
	}
}
