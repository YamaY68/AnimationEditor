#include "PoseBlender.h"

void PoseBlender::Blend(const Pose& poseA, const Pose& poseB, float weight, Pose& outPose) {
    float clampedWeight = std::clamp(weight, 0.0f, 1.0f);
    size_t count = std::min(poseA.localTransforms_.size(), poseB.localTransforms_.size());

    if (outPose.localTransforms_.size() != count) {
        outPose.Resize(count);
    }

    for (size_t i = 0; i < count; ++i) {
        outPose.localTransforms_[i] = Transform::Interpolate(
            poseA.localTransforms_[i],
            poseB.localTransforms_[i],
            clampedWeight
        );
    }
}

void PoseBlender::BlendMasked(const Pose& poseA, const Pose& poseB,
    const std::vector<float>& boneWeights, Pose& outPose) {
    size_t count = std::min(poseA.localTransforms_.size(), poseB.localTransforms_.size());

    if (outPose.localTransforms_.size() != count) {
        outPose.Resize(count);
    }

    for (size_t i = 0; i < count; ++i) {
        float w = (i < boneWeights.size()) ? std::clamp(boneWeights[i], 0.0f, 1.0f) : 0.0f;
        outPose.localTransforms_[i] = Transform::Interpolate(
            poseA.localTransforms_[i],
            poseB.localTransforms_[i],
            w
        );
    }
}