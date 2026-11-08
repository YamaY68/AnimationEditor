#include "InspectorView.h"
#include <imgui.h>
#include <DirectXMath.h>
#include <cmath>

// クォータニオンからオイラー角（度数法）を計算
static DirectX::XMFLOAT3 QuaternionToEuler(const DirectX::XMFLOAT4& q)
{
    using namespace DirectX;
    float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
    float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
    float roll = std::atan2(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (q.w * q.y - q.z * q.x);
    float pitch = (std::abs(sinp) >= 1.0f) ? std::copysign(XM_PIDIV2, sinp) : std::asin(sinp);

    float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
    float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    float yaw = std::atan2(siny_cosp, cosy_cosp);

    return { XMConvertToDegrees(pitch), XMConvertToDegrees(yaw), XMConvertToDegrees(roll) };
}

// オイラー角（度数法）からクォータニオンを計算
static DirectX::XMFLOAT4 EulerToQuaternion(const DirectX::XMFLOAT3& euler)
{
    using namespace DirectX;
    float p = XMConvertToRadians(euler.x);
    float y = XMConvertToRadians(euler.y);
    float r = XMConvertToRadians(euler.z);

    XMVECTOR q = XMQuaternionRotationRollPitchYaw(p, y, r);
    DirectX::XMFLOAT4 out;
    XMStoreFloat4(&out, q);
    return out;
}

bool InspectorView::Draw(const Skeleton& skeleton, Pose& inOutPose, AnimationClip* currentClip, float currentTime)
{
    bool keyAdded = false;

    ImGui::SetNextWindowSize(ImVec2(340, 520), ImGuiCond_FirstUseEver);
    ImGui::Begin("Inspector");

    if (skeleton.GetBoneCount() == 0)
    {
        ImGui::Text("No Skeleton Loaded");
        ImGui::End();
        return false;
    }

    // ボーン階層ツリー
    ImGui::BeginChild("BoneHierarchy", ImVec2(0, 240), true);
    ImGui::Text("Skeleton Hierarchy (%zu bones)", skeleton.GetBoneCount());
    ImGui::Separator();

    for (size_t i = 0; i < skeleton.GetBoneCount(); ++i)
    {
        if (skeleton.GetBone(i).parentIndex_ == -1)
        {
            DrawBoneNode(static_cast<int>(i), skeleton);
        }
    }
    ImGui::EndChild();

    ImGui::Separator();

    // 選択ボーンのトランスフォーム操作
    if (selectedBoneIndex_ >= 0 && selectedBoneIndex_ < static_cast<int>(skeleton.GetBoneCount()))
    {
        const auto& bone = skeleton.GetBone(selectedBoneIndex_);
        ImGui::Text("Selected: %s (ID: %d)", bone.name_.c_str(), selectedBoneIndex_);

        if (selectedBoneIndex_ < static_cast<int>(inOutPose.localTransforms_.size()))
        {
            auto& trs = inOutPose.localTransforms_[selectedBoneIndex_];

            // 1. 平行移動 (Translation)
            float t[3] = { trs.translation_.x, trs.translation_.y, trs.translation_.z };
            if (ImGui::DragFloat3("Position", t, 0.1f))
            {
                trs.translation_ = { t[0], t[1], t[2] };
            }

            // 2. 回転 (Rotation / Euler Degrees)
            DirectX::XMFLOAT3 euler = QuaternionToEuler(trs.rotation_);
            float r[3] = { euler.x, euler.y, euler.z };
            if (ImGui::DragFloat3("Rotation", r, 0.5f))
            {
                trs.rotation_ = EulerToQuaternion({ r[0], r[1], r[2] });
            }

            // 3. スケール (Scale)
            float s[3] = { trs.scale_.x, trs.scale_.y, trs.scale_.z };
            if (ImGui::DragFloat3("Scale", s, 0.01f, 0.01f, 10.0f))
            {
                trs.scale_ = { s[0], s[1], s[2] };
            }

            ImGui::Spacing();

            // 4. キーフレーム登録ボタン
            if (currentClip)
            {
                if (ImGui::Button("Key Selected Bone (K)", ImVec2(-1, 28)))
                {
                    BoneTrack* track = currentClip->GetOrCreateTrack(selectedBoneIndex_);
                    track->SetKeyframe(currentTime, trs);
                    keyAdded = true;
                }
            }
        }
    }
    else
    {
        ImGui::TextDisabled("Select a bone to edit transform.");
    }

    ImGui::End();
    return keyAdded;
}

void InspectorView::DrawBoneNode(int boneIndex, const Skeleton& skeleton)
{
    const auto& bone = skeleton.GetBone(boneIndex);

    std::vector<int> children;
    for (size_t i = 0; i < skeleton.GetBoneCount(); ++i)
    {
        if (skeleton.GetBone(i).parentIndex_ == boneIndex)
        {
            children.push_back(static_cast<int>(i));
        }
    }

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
    if (selectedBoneIndex_ == boneIndex)
    {
        flags |= ImGuiTreeNodeFlags_Selected;
    }
    if (children.empty())
    {
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    bool nodeOpen = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(boneIndex)), flags, "%s", bone.name_.c_str());

    if (ImGui::IsItemClicked())
    {
        selectedBoneIndex_ = boneIndex;
    }

    if (!children.empty() && nodeOpen)
    {
        for (int childIdx : children)
        {
            DrawBoneNode(childIdx, skeleton);
        }
        ImGui::TreePop();
    }
}