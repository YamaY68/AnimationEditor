#pragma once
#pragma once
#include <vector>
#include <algorithm>
#include "Pose.h"
#include "../Common/Transform.h"

class PoseBlender {
public:
    PoseBlender() = default;
    ~PoseBlender() = default;

    /// <summary>
    /// ２つの姿勢を線形、球面でブレンド
    /// </summary>
    /// <param name="poseA"></param>
    /// <param name="poseB"></param>
    /// <param name="weight"></param>
    /// <param name="outPose"></param>
    ///  weight = 0.0f で poseA 100%、weight = 1.0f で poseB 100%
    static void Blend(const Pose& poseA, const Pose& poseB, float weight, Pose& outPose);


    /// <summary>
    /// ボーン事のマスクウェイトを用いたレイヤーブレンド
    /// </summary>
    /// <param name="poseA"></param>
    /// <param name="poseB"></param>
    /// <param name="boneWeights"></param>
    /// <param name="outPose"></param>
    static void BlendMasked(const Pose& poseA, const Pose& poseB,
        const std::vector<float>& boneWeights, Pose& outPose);
};