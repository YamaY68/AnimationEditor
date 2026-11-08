#pragma once
#include <vector>
#include <string>
#include "../Animation/Skeleton.h"
#include "../Animation/Pose.h"
#include"../Animation/AnimationClip.h"

class InspectorView
{
public:
    InspectorView() = default;
    ~InspectorView() = default;

    // ボーン階層ツリーと現在のローカル姿勢（TRS）を表示
    bool Draw(const Skeleton& skeleton, Pose& inOutPose, AnimationClip* currentClip, float currentTime);

    // 選択中のボーンインデックスを取得（未選択時は -1）
    int GetSelectedBoneIndex() const { return selectedBoneIndex_; }

private:
    void DrawBoneNode(int boneIndex, const Skeleton& skeleton);

    int selectedBoneIndex_ = -1;
};