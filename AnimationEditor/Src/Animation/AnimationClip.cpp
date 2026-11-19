#include "AnimationClip.h"
#include<DirectXMath.h>
#include<utility>
using namespace DirectX;

float AnimationClip::WrapTime(float time) const
{
    float t = time;
    //I’[‚¿‚å‚¤‚Ç(time == duration_)‚ÍÜ‚è•Ô‚³‚¸ÅIƒtƒŒ[ƒ€‚ð•\Ž¦‚·‚é
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
    //‚Ü‚¸Œ³‚ÌŽp¨‚ð‹‚ß‚é
    SampleBase(time, skeleton, outPose);

    //C³•ª‚ðd‚Ë‚é(ƒL[‚Ì–³‚¢ƒ{[ƒ“‚ÍC³‚È‚µ‚»‚Ì‚Ü‚Ü)
    float evalTime = WrapTime(time);
    const Transform identity;   //ˆÚ“®0E‰ñ“]‚È‚µEƒXƒP[ƒ‹1
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
    //Out“_‚ªIn“_‚æ‚è‘O‚È‚ç“ü‚ê‘Ö‚¦‚é
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
    //ˆÚ“®F‘«‚µŽZ
    XMStoreFloat3(&r.translation_, XMVectorAdd(XMLoadFloat3(&base.translation_), XMLoadFloat3(&offset.translation_)));
    //‰ñ“]FŒ³‚Ì‰ñ“]‚Ì‚ ‚Æ‚ÉC³•ª‚Ì‰ñ“]‚ð‚©‚¯‚é
    XMVECTOR q = XMQuaternionMultiply(XMLoadFloat4(&base.rotation_), XMLoadFloat4(&offset.rotation_));
    XMStoreFloat4(&r.rotation_, XMQuaternionNormalize(q));
    //ƒXƒP[ƒ‹FŠ|‚¯ŽZ
    XMStoreFloat3(&r.scale_, XMVectorMultiply(XMLoadFloat3(&base.scale_), XMLoadFloat3(&offset.scale_)));
    return r;
}

Transform AnimationClip::Difference(const Transform& base, const Transform& target)
{
    Transform r;
    //ˆÚ“®F–Ú•W|Œ³
    XMStoreFloat3(&r.translation_, XMVectorSubtract(XMLoadFloat3(&target.translation_), XMLoadFloat3(&base.translation_)));
    //‰ñ“]FŒ³‚Ì‰ñ“]‚ð‘Å‚¿Á‚µ‚Ä‚©‚ç–Ú•W‚Ì‰ñ“]‚Ö
    XMVECTOR q = XMQuaternionMultiply(XMQuaternionInverse(XMLoadFloat4(&base.rotation_)), XMLoadFloat4(&target.rotation_));
    XMStoreFloat4(&r.rotation_, XMQuaternionNormalize(q));
    //ƒXƒP[ƒ‹F–Ú•W€Œ³(0œŽZ‚ð”ð‚¯‚é)
    auto safeDiv = [](float t, float b) { return (std::abs(b) > 1e-6f) ? t / b : 1.0f; };
    r.scale_ = {
        safeDiv(target.scale_.x, base.scale_.x),
        safeDiv(target.scale_.y, base.scale_.y),
        safeDiv(target.scale_.z, base.scale_.z) };
    return r;
}