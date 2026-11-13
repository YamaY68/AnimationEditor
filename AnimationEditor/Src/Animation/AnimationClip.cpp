#include "AnimationClip.h"

BoneTrack* AnimationClip::GetOrCreateTrack(int boneIndex)
{
    //既存のトラック配列に対象ボーンがあるか探索
    for (auto& track : boneTracks_)
    {
        if (track.boneIndex_ == boneIndex)
        {
            return&track;
        }
    }
    //存在しない場合は末尾に新設
    boneTracks_.emplace_back();
    boneTracks_.back().boneIndex_ = boneIndex;
    return &boneTracks_.back();
}

void AnimationClip::Sample(float time, const Skeleton& skeleton, Pose& outPose) const
{
    float evalTime = time;
    //再生時間を0.0f～durationのループ区間に正規化
    if (duration_ > 0.0f)
    {
        evalTime = std::fmod(time, duration_);
        //逆再生等で負の値になった場合の正回転補正
        if (evalTime < 0.0f)
        {
            evalTime += duration_;
        }
    }
    //登録されている全ボーンのトラックを操作して姿勢を評価
    for (const auto& track : boneTracks_)
    {
        int bIdx = track.boneIndex_;

        if (bIdx >= 0 && bIdx < static_cast<int>(outPose.GetBoneCoount()))
        {
            const auto&bindTransform=skeleton.GetBone(bIdx).bindPoseLocal_;

            outPose[bIdx] = track.Sample(evalTime, bindTransform);
        }
    }

}
