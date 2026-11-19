#include "AnimationClip.h"
#include<DirectXMath.h>
#include<cmath>
#include<utility>

using namespace DirectX;

namespace
{
	//スケールの割り算で、これより小さい値は0とみなす
	constexpr float kScaleEpsilon = 1e-6f;
}

//=============================================================
// トラックの取得
//=============================================================

const BoneTrack* AnimationClip::FindTrack(const std::vector<BoneTrack>& tracks, int boneIndex)
{
	for (const auto& track : tracks)
	{
		if (track.boneIndex_ == boneIndex) return &track;
	}
	return nullptr;
}

BoneTrack* AnimationClip::FindTrack(std::vector<BoneTrack>& tracks, int boneIndex)
{
	//const版と同じ処理なので、const版を呼んで結果のconstを外す
	return const_cast<BoneTrack*>(FindTrack(static_cast<const std::vector<BoneTrack>&>(tracks), boneIndex));
}

BoneTrack* AnimationClip::GetOrCreate(std::vector<BoneTrack>& tracks, int boneIndex)
{
	if (BoneTrack* track = FindTrack(tracks, boneIndex))
	{
		return track;
	}
	tracks.emplace_back();
	tracks.back().boneIndex_ = boneIndex;
	return &tracks.back();
}

BoneTrack* AnimationClip::GetOrCreateTrack(int boneIndex)
{
	return GetOrCreate(boneTracks_, boneIndex);
}

BoneTrack* AnimationClip::GetOrCreateOffsetTrack(int boneIndex)
{
	return GetOrCreate(offsetTracks_, boneIndex);
}

const BoneTrack* AnimationClip::FindOffsetTrack(int boneIndex) const
{
	return FindTrack(offsetTracks_, boneIndex);
}

//=============================================================
// 姿勢の計算
//=============================================================

float AnimationClip::WrapTime(float time) const
{
	float t = time;
	//終端ちょうど(time == duration_)は折り返さず、最終フレームを表示する
	if (duration_ > 0.0f && (t > duration_ || t < 0.0f))
	{
		t = std::fmod(t, duration_);
		if (t < 0.0f) t += duration_;
	}
	return t;
}

void AnimationClip::SampleBase(float time, const Skeleton& skeleton, Pose& outPose) const
{
	const float evalTime = WrapTime(time);
	for (const auto& track : boneTracks_)
	{
		const int boneIndex = track.boneIndex_;
		if (boneIndex >= 0 && boneIndex < static_cast<int>(outPose.GetBoneCount()))
		{
			//キーが無い値は、バインドポーズの値を使う
			const Transform& bindPose = skeleton.GetBone(boneIndex).bindPoseLocal_;
			outPose[boneIndex] = track.Sample(evalTime, bindPose);
		}
	}
}

void AnimationClip::Sample(float time, const Skeleton& skeleton, Pose& outPose) const
{
	//まず元の動きの姿勢を求める
	SampleBase(time, skeleton, outPose);

	//その上に修正分を重ねる(修正分のトラックが無いボーンは元の動きのまま)
	const float evalTime = WrapTime(time);
	const Transform noOffset;	//キーが無い値は「修正なし」として扱う
	for (const auto& track : offsetTracks_)
	{
		const int boneIndex = track.boneIndex_;
		if (boneIndex >= 0 && boneIndex < static_cast<int>(outPose.GetBoneCount()))
		{
			Transform offset = track.Sample(evalTime, noOffset);
			outPose[boneIndex] = Combine(outPose[boneIndex], offset);
		}
	}
}

Transform AnimationClip::SampleBaseBone(int boneIndex, float time, const Skeleton& skeleton) const
{
	const Transform& bindPose = skeleton.GetBone(boneIndex).bindPoseLocal_;
	const BoneTrack* track = FindTrack(boneTracks_, boneIndex);
	return track ? track->Sample(WrapTime(time), bindPose) : bindPose;
}

//=============================================================
// 修正分のキーの編集
//=============================================================

void AnimationClip::SetOffsetKey(int boneIndex, float time, const Transform& targetPose, const Skeleton& skeleton)
{
	const Transform base = SampleBaseBone(boneIndex, time, skeleton);
	GetOrCreateOffsetTrack(boneIndex)->SetKeyframe(time, Difference(base, targetPose));
}

void AnimationClip::ReplaceOffsetRange(int boneIndex, float startTime, float endTime,
	Transform startPose, Transform endPose, const Skeleton& skeleton)
{
	//終点が始点より前なら入れ替える
	if (startTime > endTime)
	{
		std::swap(startTime, endTime);
		std::swap(startPose, endPose);
	}
	const Transform startOffset = Difference(SampleBaseBone(boneIndex, startTime, skeleton), startPose);
	const Transform endOffset = Difference(SampleBaseBone(boneIndex, endTime, skeleton), endPose);
	GetOrCreateOffsetTrack(boneIndex)->ReplaceRange(startTime, endTime, startOffset, endOffset);
}

bool AnimationClip::HasOffsetKey(int boneIndex, float time) const
{
	const BoneTrack* track = FindOffsetTrack(boneIndex);
	return track != nullptr && track->HasKeyframe(time);
}

bool AnimationClip::RemoveOffsetKey(int boneIndex, float time)
{
	BoneTrack* track = FindTrack(offsetTracks_, boneIndex);
	return track != nullptr && track->RemoveKeyframe(time);
}

void AnimationClip::SetZeroOffsetKey(int boneIndex, float time)
{
	//Transformの初期値は「移動0・回転なし・スケール1」＝修正なし
	GetOrCreateOffsetTrack(boneIndex)->SetKeyframe(time, Transform{});
}

bool AnimationClip::GetOffsetInterpolation(int boneIndex, float time, InterpolationType& out) const
{
	const BoneTrack* track = FindOffsetTrack(boneIndex);
	return track != nullptr && track->GetInterpolation(time, out);
}

void AnimationClip::SetOffsetInterpolation(int boneIndex, float time, InterpolationType type)
{
	if (BoneTrack* track = FindTrack(offsetTracks_, boneIndex))
	{
		track->SetInterpolation(time, type);
	}
}

//=============================================================
// 元の動きと修正分の計算
//=============================================================

Transform AnimationClip::Combine(const Transform& base, const Transform& offset)
{
	Transform r;
	//移動：足し算
	XMStoreFloat3(&r.translation_,
		XMVectorAdd(XMLoadFloat3(&base.translation_), XMLoadFloat3(&offset.translation_)));

	//回転：元の回転のあとに修正分の回転をかける
	//(XMQuaternionMultiply(A, B) は「Aで回してからBで回す」)
	XMVECTOR q = XMQuaternionMultiply(XMLoadFloat4(&base.rotation_), XMLoadFloat4(&offset.rotation_));
	XMStoreFloat4(&r.rotation_, XMQuaternionNormalize(q));

	//スケール：掛け算
	XMStoreFloat3(&r.scale_,
		XMVectorMultiply(XMLoadFloat3(&base.scale_), XMLoadFloat3(&offset.scale_)));
	return r;
}

Transform AnimationClip::Difference(const Transform& base, const Transform& target)
{
	Transform r;
	//移動：目標－元
	XMStoreFloat3(&r.translation_,
		XMVectorSubtract(XMLoadFloat3(&target.translation_), XMLoadFloat3(&base.translation_)));

	//回転：元の回転を打ち消してから、目標の回転をかける
	XMVECTOR q = XMQuaternionMultiply(
		XMQuaternionInverse(XMLoadFloat4(&base.rotation_)), XMLoadFloat4(&target.rotation_));
	XMStoreFloat4(&r.rotation_, XMQuaternionNormalize(q));

	//スケール：目標÷元(0で割らないようにする)
	auto safeDiv = [](float t, float b) { return (std::abs(b) > kScaleEpsilon) ? t / b : 1.0f; };
	r.scale_ = {
		safeDiv(target.scale_.x, base.scale_.x),
		safeDiv(target.scale_.y, base.scale_.y),
		safeDiv(target.scale_.z, base.scale_.z) };
	return r;
}