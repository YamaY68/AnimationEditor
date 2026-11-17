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

	//スキニングバレット行列の事前計算
	//頂点ごとに毎回逆行列を掛けると重いので、ボーン数分だけ先に計算
	std::vector<XMMATRIX>skinMatrices(boneCount);
	for (size_t b = 0; b < boneCount; ++b)
	{
		if (b < worldMatrices.size())
		{
			//頂点をボーンの初期位置を原点とする空間へ引き戻す逆行列を生成
			XMMATRIX invBind = XMLoadFloat4x4(&skeleton.GetBone(b).invBindMatrix_);
			//現在のアニメーション姿勢でのボーンのワールド位置回転を表す行列
			XMMATRIX currentWorld = XMLoadFloat4x4(&worldMatrices[b]);
			//２つをかけ合わせる
			skinMatrices[b] = XMMatrixMultiply(invBind, currentWorld);
		}
		else
		{
			skinMatrices[b] = XMMatrixIdentity();
		}
	}
	
	//全頂点の座標と法線を最大４ボーンの重みでブレンド
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
			if (weight <= 0.0001f)continue;	//影響度０のボーンは計算をスキップ

			int bIdx = src.boneIndices_[k];
			if (bIdx >= 0 && bIdx < static_cast<int>(boneCount))
			{
				//座標の変換
				XMVECTOR transformedPos = XMVector3TransformCoord(srcPos, skinMatrices[bIdx]);
				//法線の返還
				XMVECTOR transformNorm = XMVector3TransformNormal(srcNorm, skinMatrices[bIdx]);
				//ウェイトをかけて加算
				skinnedPos = XMVectorAdd(skinnedPos, XMVectorScale(transformedPos, weight));
				skinnedNorm = XMVectorAdd(skinnedNorm, XMVectorScale(transformNorm, weight));
			}
		}


		//DxLibの描画頂点バッファへ書き込み
		XMFLOAT3 outPos, outNorm;
		XMStoreFloat3(&outPos, skinnedPos);
		XMStoreFloat3(&outNorm, XMVector3Normalize(skinnedNorm));
		//DxLibのVertex３D構造体に流す
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
	if (skinnedVertices_.empty()) return;

	SetTextureAddressMode(DX_TEXADDRESS_WRAP);
	SetDrawBlendMode(DX_BLENDMODE_NOBLEND, 255);

	for (const auto& sub : subMeshes_)
	{
		if (sub.indexCount_ == 0) continue;
		size_t vertexOffset = static_cast<size_t>(sub.startIndex_);
		if (vertexOffset >= skinnedVertices_.size()) continue;

		int polygonCount = static_cast<int>(sub.indexCount_ / 3);
		int tex = (sub.textureHandle_ != -1) ? sub.textureHandle_ : DX_NONE_GRAPH;

		DrawPolygon3D(
			const_cast<VERTEX3D*>(&skinnedVertices_[vertexOffset]),
			polygonCount,
			tex,
			FALSE
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
