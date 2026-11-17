#include "Skeleton.h"
#include<DxLib.h>
using namespace DirectX;
int Skeleton::AddBone(const std::string& name, int parentIndex, const Transform& localBindPose)
{
    int index = static_cast<int>(bones_.size());

    Bone bone;
    bone.name_ = name;
    bone.parentIndex_ = parentIndex;
    bone.bindPoseLocal_ = localBindPose;
    DirectX::XMStoreFloat4x4(&bone.invBindMatrix_, DirectX::XMMatrixIdentity());

    bones_.push_back(bone);
    nameToIndex_[name] = index;

    return index;
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
        pose[i] = bones_[i].bindPoseLocal_;
    }
    return pose;
}

void Skeleton::ComputeInverseBindMatrices(void)
{
    const size_t count = bones_.size();
    if (count == 0)return;
    //初期ポーズでの全ボーンのワールド行列を計算する
    Pose bindPose = CreateBindPose();
    std::vector<XMFLOAT4X4>bindWorld;
    ComputeWorldMatrices(bindPose, bindWorld);
    //各ボーンのワールド行列の逆行列を求めて保存する
    for (size_t i = 0; i < count; ++i)
    {
        XMMATRIX w = XMLoadFloat4x4(&bindWorld[i]);
        XMVECTOR det;
        XMMATRIX inv = XMMatrixInverse(&det, w);
        XMStoreFloat4x4(&bones_[i].invBindMatrix_, inv);
    }
}

void Skeleton::ComputeWorldMatrices(const Pose& localPose, std::vector<DirectX::XMFLOAT4X4>& outWorldMatrices) const
{
    const size_t count = bones_.size();

    if (outWorldMatrices.size() != count)
    {
        outWorldMatrices.resize(count);
    }

    std::vector<XMMATRIX>worldMats(count);

    for (size_t i = 0; i < count; ++i)
    {
        //自分のローカルTRSを4x4行列に変換
        XMMATRIX local = localPose[i].ToMatrix();

        int p = bones_[i].parentIndex_;
        if (p < 0)
        {
            //ルートボーン無し：自分のローカル行列がそのままワールド行列になる
            worldMats[i] = local;
        }
        else
        {
            //子ボーン：自分のローカル行列x親のワールド行列で親の動きを引き継ぐ
            worldMats[i] = XMMatrixMultiply(local, worldMats[p]);
        }

        XMStoreFloat4x4(&outWorldMatrices[i], worldMats[i]);
    }
}

void Skeleton::DrawDebugBones(const std::vector<DirectX::XMFLOAT4X4>& worldMatrices, int boneColor, int jointColor) const
{
    const size_t count = bones_.size();
    for (size_t i = 0; i < count; ++i)
    {
        VECTOR currentPos = VGet(worldMatrices[i]._41, worldMatrices[i]._42, worldMatrices[i]._43);

        DrawSphere3D(currentPos, 0.4f, 8, jointColor, jointColor, TRUE);

        int p = bones_[i].parentIndex_;
        if (p >= 0 && p < static_cast<int>(worldMatrices.size()))
        {
            VECTOR parentPos = VGet(worldMatrices[p]._41, worldMatrices[p]._42, worldMatrices[p]._43);
            DrawLine3D(parentPos, currentPos, boneColor);
        }
    }
}
