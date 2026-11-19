#include "AnimationClip.h"
#include<DirectXMath.h>
#include<utility>
using namespace DirectX;

float AnimationClip::WrapTime(float time) const
{
    float t = time;
    //終端ちょうど(time == duration_)は折り返さず最終フレームを表示する
    if (duration_ > 0.0f && (t > duration_ || t < 0.0f))
    {
        t = std::fmod(t, duration_);
        if (t < 0.0f) t += duration_;
    }
    return t;
}

const BoneTrack* AnimationClip::FindTrack(const std::vector<BoneTrack>& tracks, int boneIndex)
{
    for (const auto& track : tracks)
    {
        if (track.boneIndex_ == boneIndex) return &track;
    }
    return nullptr;
}

BoneTrack* AnimationClip::GetOrCreateTrack(int boneIndex)
{
    for (auto& track : boneTracks_)
    {
        if (track.boneIndex_ == boneIndex) return &track;
    }
    boneTracks_.emplace_back();
    boneTracks_.back().boneIndex_ = boneIndex;
    return &boneTracks_.back();
}

BoneTrack* AnimationClip::GetOrCreateOffsetTrack(int boneIndex)
{
    for (auto& track : offsetTracks_)
    {
        if (track.boneIndex_ == boneIndex) return &track;
    }
    offsetTracks_.emplace_back();
    offsetTracks_.back().boneIndex_ = boneIndex;
    return &offsetTracks_.back();
}

const BoneTrack* AnimationClip::FindOffsetTrack(int boneIndex) const
{
    return FindTrack(offsetTracks_, boneIndex);
}

void AnimationClip::SampleBase(float time, const Skeleton& skeleton, Pose& outPose) const
{
    float evalTime = WrapTime(time);
    for (const auto& track : boneTracks_)
    {
        int bIdx = track.boneIndex_;
        if (bIdx >= 0 && bIdx < static_cast<int>(outPose.GetBoneCoount()))
        {
            const auto& bindTransform = skeleton.GetBone(bIdx).bindPoseLocal_;
            outPose[bIdx] = track.Sample(evalTime, bindTransform);
        }
    }
}

void AnimationClip::Sample(float time, const Skeleton& skeleton, Pose& outPose) const
{
    //まず元の姿勢を求める
    SampleBase(time, skeleton, outPose);

    //修正分を重ねる(キーの無いボーンは修正なし＝そのまま)
    float evalTime = WrapTime(time);
    const Transform identity;   //移動0・回転なし・スケール1
    for (const auto& track : offsetTracks_)
    {
        int bIdx = track.boneIndex_;
        if (bIdx >= 0 && bIdx < static_cast<int>(outPose.GetBoneCoount()))
        {
            Transform offset = track.Sample(evalTime, identity);
            outPose[bIdx] = Combine(outPose[bIdx], offset);
        }
    }
}

Transform AnimationClip::SampleBaseBone(int boneIndex, float time, const Skeleton& skeleton) const
{
    const Transform& bind = skeleton.GetBone(boneIndex).bindPoseLocal_;
    const BoneTrack* track = FindTrack(boneTracks_, boneIndex);
    return track ? track->Sample(WrapTime(time), bind) : bind;
}

void AnimationClip::SetOffsetKey(int boneIndex, float time, const Transform& targetPose, const Skeleton& skeleton)
{
    Transform base = SampleBaseBone(boneIndex, time, skeleton);
    GetOrCreateOffsetTrack(boneIndex)->SetKeyframe(time, Difference(base, targetPose));
}

void AnimationClip::ReplaceOffsetRange(int boneIndex, float startTime, float endTime,
    Transform startPose, Transform endPose, const Skeleton& skeleton)
{
    //Out点がIn点より前なら入れ替える
    if (startTime > endTime)
    {
        std::swap(startTime, endTime);
        std::swap(startPose, endPose);
    }
    Transform startOffset = Difference(SampleBaseBone(boneIndex, startTime, skeleton), startPose);
    Transform endOffset = Difference(SampleBaseBone(boneIndex, endTime, skeleton), endPose);
    GetOrCreateOffsetTrack(boneIndex)->ReplaceRange(startTime, endTime, startOffset, endOffset);
}

Transform AnimationClip::Combine(const Transform& base, const Transform& offset)
{
    Transform r;
    //移動：足し算
    XMStoreFloat3(&r.translation_, XMVectorAdd(XMLoadFloat3(&base.translation_), XMLoadFloat3(&offset.translation_)));
    //回転：元の回転のあとに修正分の回転をかける
    XMVECTOR q = XMQuaternionMultiply(XMLoadFloat4(&base.rotation_), XMLoadFloat4(&offset.rotation_));
    XMStoreFloat4(&r.rotation_, XMQuaternionNormalize(q));
    //スケール：掛け算
    XMStoreFloat3(&r.scale_, XMVectorMultiply(XMLoadFloat3(&base.scale_), XMLoadFloat3(&offset.scale_)));
    return r;
}

Transform AnimationClip::Difference(const Transform& base, const Transform& target)
{
    Transform r;
    //移動：目標－元
    XMStoreFloat3(&r.translation_, XMVectorSubtract(XMLoadFloat3(&target.translation_), XMLoadFloat3(&base.translation_)));
    //回転：元の回転を打ち消してから目標の回転へ
    XMVECTOR q = XMQuaternionMultiply(XMQuaternionInverse(XMLoadFloat4(&base.rotation_)), XMLoadFloat4(&target.rotation_));
    XMStoreFloat4(&r.rotation_, XMQuaternionNormalize(q));
    //スケール：目標÷元(0除算を避ける)
    auto safeDiv = [](float t, float b) { return (std::abs(b) > 1e-6f) ? t / b : 1.0f; };
    r.scale_ = {
        safeDiv(target.scale_.x, base.scale_.x),
        safeDiv(target.scale_.y, base.scale_.y),
        safeDiv(target.scale_.z, base.scale_.z) };
    return r;
}

bool AnimationClip::RemoveOffsetKey(int boneIndex, float time)
{
    for (auto& track : offsetTracks_)
    {
        if (track.boneIndex_ == boneIndex) return track.RemoveKeyframe(time);
    }
    return false;
}

void AnimationClip::SetZeroOffsetKey(int boneIndex, float time)
{
    //Transformの初期値は「移動0・回転なし・スケール1」＝修正なし
    GetOrCreateOffsetTrack(boneIndex)->SetKeyframe(time, Transform{});
}

bool AnimationClip::HasOffsetKey(int boneIndex, float time) const
{
    const BoneTrack* track = FindOffsetTrack(boneIndex);
    if (!track) return false;

    auto hasKey = [time](const auto& keys)
        {
            for (const auto& k : keys)
            {
                if (std::abs(k.time_ - time) < 0.001f) return true;
            }
            return false;
        };
    return hasKey(track->translationTrack_.keys_)
        || hasKey(track->rotationTrack_.keys_)
        || hasKey(track->scaleTrack_.keys_);
}

bool AnimationClip::GetOffsetInterpolation(int boneIndex, float time, InterpolationType& out) const
{
    const BoneTrack* track = FindOffsetTrack(boneIndex);
    return track != nullptr && track->GetInterpolation(time, out);
}

void AnimationClip::SetOffsetInterpolation(int boneIndex, float time, InterpolationType type)
{
    for (auto& track : offsetTracks_)
    {
        if (track.boneIndex_ == boneIndex)
        {
            track.SetInterpolation(time, type);
            return;
        }
    }
}