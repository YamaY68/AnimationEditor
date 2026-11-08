#include "Skeleton.h"
#include<DxLib.h>

int Skeleton::AddBone(const std::string& name, int parentIndex, const Transform& localBindPose)
{
    int newIndex = static_cast<int>(bones_.size());

    Bone bone;
    bone.name_ = name;
    bone.parentIndex_ = parentIndex;
    bone.bindPoseLocal_ = localBindPose;

    DirectX::XMStoreFloat4x4(&bone.invBindMatrix_, DirectX::XMMatrixIdentity());

    bones_.push_back(bone);
    nameToIndex_[name] = newIndex;

    return newIndex;
}

int Skeleton::FindBoneIndex(const std::string& name) const
{
    auto it = nameToIndex_.find(name);
    if (it != nameToIndex_.end())
    {
        return it->second;
    }
    return -1;
}

Pose Skeleton::CreateBindPose(void) const
{
    Pose pose;
    pose.Resize(bones_.size());
    for (size_t i = 0; i < bones_.size(); ++i)
    {
        pose.localTransforms_[i] = bones_[i].bindPoseLocal_;
    }
    return pose;
}

void Skeleton::ComputeInverseBindMatrices(void)
{
    using namespace DirectX;
    const size_t boneCount = bones_.size();
    if (boneCount == 0) return;

    // バインドポーズ（Tポーズ）における各ボーンのワールド行列を計算
    Pose bindPose = CreateBindPose();
    std::vector<XMFLOAT4X4> bindWorldMatrices(boneCount);
    ComputeWorldMatrices(bindPose, bindWorldMatrices);

    // 各ボーンのワールド行列の逆行列（Inverse Bind Matrix）を算出・保持
    for (size_t i = 0; i < boneCount; ++i)
    {
        XMMATRIX worldMat = XMLoadFloat4x4(&bindWorldMatrices[i]);
        XMVECTOR det;
        XMMATRIX invWorldMat = XMMatrixInverse(&det, worldMat);
        XMStoreFloat4x4(&bones_[i].invBindMatrix_, invWorldMat);
    }
}

void Skeleton::ComputeWorldMatrices(const Pose& localPose, std::vector<DirectX::XMFLOAT4X4>& outWorldMatrices) const
{
    using namespace DirectX;
    const size_t boneCount = bones_.size();
    if (outWorldMatrices.size() < boneCount)
    {
        outWorldMatrices.resize(boneCount);
    }

    for (size_t i = 0; i < boneCount; ++i)
    {
        XMMATRIX localMat = localPose.localTransforms_[i].ToMatrix();
        int parentIdx = bones_[i].parentIndex_;

        if (parentIdx < 0)
        {
            XMStoreFloat4x4(&outWorldMatrices[i], localMat);
        }
        else
        {
            XMMATRIX parentWorldMat = XMLoadFloat4x4(&outWorldMatrices[parentIdx]);
            XMMATRIX worldMat = XMMatrixMultiply(localMat, parentWorldMat);
            XMStoreFloat4x4(&outWorldMatrices[i], worldMat);
        }
    }
}


void Skeleton::DrawDebugBones(const std::vector<DirectX::XMFLOAT4X4>& worldMatrices, int boneColor, int jointColor) const
{
    const size_t boneCount = bones_.size();
    for (size_t i = 0; i < boneCount; ++i) {
        // ワールド行列の第4行（平行移動要素）から関節座標を取得
        VECTOR jointPos = VGet(worldMatrices[i]._41, worldMatrices[i]._42, worldMatrices[i]._43);

        // 関節の点を描画
        DrawSphere3D(jointPos, 0.4f, 8, jointColor, jointColor, TRUE);

        // 親ボーンが存在する場合、親と子の間を直線で結んで骨格を描画
        int parentIdx = bones_[i].parentIndex_;
        if (parentIdx >= 0) {
            VECTOR parentPos = VGet(worldMatrices[parentIdx]._41, worldMatrices[parentIdx]._42, worldMatrices[parentIdx]._43);
            DrawLine3D(parentPos, jointPos, boneColor);
        }
    }
}
