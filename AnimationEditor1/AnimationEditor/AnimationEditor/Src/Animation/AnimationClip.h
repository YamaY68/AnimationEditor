#pragma once
#include<string>
#include<vector>
#include<cmath>

#include"AnimationTrack.h"
#include"Skeleton.h"

class AnimationClip
{
public:
    std::string name_;
    float duration_ = 0.0f;               // アニメーション全体の長さ（秒）
    std::vector<BoneTrack> boneTracks_;   // ボーンごとのTRSトラック
    
    AnimationClip() = default;
    AnimationClip(const std::string& name, float duration)
        : name_(name), duration_(duration) {
    }

    // ボーントラックの取得または新規作成
    BoneTrack* GetOrCreateTrack(int boneIndex) {
        for (auto& track : boneTracks_) {
            if (track.boneIndex_ == boneIndex) return &track;
        }
        boneTracks_.emplace_back();
        boneTracks_.back().boneIndex_ = boneIndex;
        return &boneTracks_.back();
    }

    void Sample(float time, const Skeleton& skeleton, Pose& outPose)const
    {
        float evalTime = time;

        if (duration_ > 0.0f)
        {
            evalTime = std::fmod(time, duration_);
            if (evalTime < 0.0f)
            {
                evalTime += duration_;
            }
        }

        for (const auto& track : boneTracks_)
        {
            int bIdx = track.boneIndex_;
            if (bIdx >= 0 && bIdx < static_cast<int>(outPose.localTransforms_.size()))
            {
                const auto& bindTransform = skeleton.GetBone(bIdx).bindPoseLocal_;
                outPose.localTransforms_[bIdx] = track.Sample(evalTime, bindTransform);
            }
        }
    }
};