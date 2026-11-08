#pragma once
#include <vector>
#include <string>
#include <algorithm>
#include <DxLib.h>
#include <DirectXMath.h>
#include "../Animation/Skeleton.h"

// スキニング用頂点（最大4ボーンの影響）
struct SkinVertex
{
    DirectX::XMFLOAT3 position_ = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 normal_ = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT2 uv_ = { 0.0f, 0.0f };
    float boneWeights_[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int boneIndices_[4] = { 0, 0, 0, 0 };
};

// サブメッシュ情報（マテリアル／テクスチャ単位）
struct SubMesh
{
    unsigned int indexOffset_ = 0; // インデックスバッファの開始位置
    unsigned int indexCount_ = 0; // インデックス数
    int textureHandle_ = -1; // DxLibのテクスチャハンドル
};

class FbxMeshModel
{
public:
    std::vector<SkinVertex> sourceVertices_;     // 原点・バインド姿勢の頂点データ
    std::vector<unsigned short> indices_;       // インデックスバッファ
    std::vector<VERTEX3D> skinnedVertices_;     // DxLib描画用バッファ
    std::vector<SubMesh> subMeshes_;            // サブメッシュリスト

    ~FbxMeshModel()
    {
        ReleaseTextures();
    }

    void ReleaseTextures()
    {
        for (auto& sm : subMeshes_)
        {
            if (sm.textureHandle_ != -1)
            {
                DeleteGraph(sm.textureHandle_);
                sm.textureHandle_ = -1;
            }
        }
    }

    void PrepareBuffer(void)
    {
        skinnedVertices_.resize(sourceVertices_.size());
        for (size_t i = 0; i < sourceVertices_.size(); ++i)
        {
            skinnedVertices_[i].u = sourceVertices_[i].uv_.x;
            skinnedVertices_[i].v = sourceVertices_[i].uv_.y;
            skinnedVertices_[i].su = 0.0f;
            skinnedVertices_[i].sv = 0.0f;
            // 白に近い明るいグレー (R:210, G:210, B:215, A:255)
            skinnedVertices_[i].dif = GetColorU8(210, 210, 215, 255);
            skinnedVertices_[i].spc = GetColorU8(0, 0, 0, 0);
        }
    }

    // CPUスキニングを実行して DxLib の VERTEX3D バッファを更新
    void UpdateSkinning(const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& currentWorldMatrices)
    {
        using namespace DirectX;
        const size_t boneCount = skeleton.GetBoneCount();
        if (boneCount == 0 || currentWorldMatrices.size() < boneCount) return;

        // スキニングパレット行列 (M_invBind * M_world) を事前計算
        std::vector<XMMATRIX> skinningPalette(boneCount);
        for (size_t b = 0; b < boneCount; ++b)
        {
            XMMATRIX invBind = XMLoadFloat4x4(&skeleton.GetBone(b).invBindMatrix_);
            XMMATRIX world = XMLoadFloat4x4(&currentWorldMatrices[b]);
            skinningPalette[b] = XMMatrixMultiply(invBind, world);
        }

        const size_t vertexCount = sourceVertices_.size();
        for (size_t i = 0; i < vertexCount; ++i)
        {
            const auto& src = sourceVertices_[i];
            XMVECTOR srcPos = XMLoadFloat3(&src.position_);
            XMVECTOR srcNorm = XMLoadFloat3(&src.normal_);

            XMVECTOR finalPos = XMVectorZero();
            XMVECTOR finalNorm = XMVectorZero();

            for (int k = 0; k < 4; ++k)
            {
                float weight = src.boneWeights_[k];
                if (weight > 0.0f)
                {
                    int bIdx = src.boneIndices_[k];
                    if (bIdx >= 0 && bIdx < static_cast<int>(boneCount))
                    {
                        const XMMATRIX& mat = skinningPalette[bIdx];
                        finalPos = XMVectorAdd(finalPos, XMVectorScale(XMVector3TransformCoord(srcPos, mat), weight));
                        finalNorm = XMVectorAdd(finalNorm, XMVectorScale(XMVector3TransformNormal(srcNorm, mat), weight));
                    }
                }
            }

            XMFLOAT3 fPos, fNorm;
            XMStoreFloat3(&fPos, finalPos);
            XMStoreFloat3(&fNorm, XMVector3Normalize(finalNorm));

            skinnedVertices_[i].pos = VGet(fPos.x, fPos.y, fPos.z);
            skinnedVertices_[i].norm = VGet(fNorm.x, fNorm.y, fNorm.z);
        }
    }

    void Draw()
    {
        if (skinnedVertices_.empty()) return;

        SetUseBackCulling(FALSE);

        for (const auto& subMesh : subMeshes_)
        {
            if (subMesh.indexCount_ == 0) continue;

            size_t vertexOffset = subMesh.indexOffset_;
            if (vertexOffset >= skinnedVertices_.size()) continue;

            int polygonCount = static_cast<int>(subMesh.indexCount_ / 3);

            // テクスチャは使わず、ライトグレーの単色メッシュとして描画
            DrawPolygon3D(
                &skinnedVertices_[vertexOffset],
                polygonCount,
                DX_NONE_GRAPH,
                FALSE
            );
        }

        SetUseBackCulling(TRUE);
    }
    void ResetToBindPose(void)
    {
        if (skinnedVertices_.size() != sourceVertices_.size())
        {
            PrepareBuffer();
        }

        for (size_t i = 0; i < sourceVertices_.size(); ++i)
        {
            skinnedVertices_[i].pos = VGet(
                sourceVertices_[i].position_.x,
                sourceVertices_[i].position_.y,
                sourceVertices_[i].position_.z
            );
            skinnedVertices_[i].norm = VGet(
                sourceVertices_[i].normal_.x,
                sourceVertices_[i].normal_.y,
                sourceVertices_[i].normal_.z
            );
            skinnedVertices_[i].u = sourceVertices_[i].uv_.x;
            skinnedVertices_[i].v = sourceVertices_[i].uv_.y;
            skinnedVertices_[i].dif = GetColorU8(255, 255, 255, 255); // 不透明な白色をセット
            skinnedVertices_[i].spc = GetColorU8(0, 0, 0, 0);
        }
    }
};