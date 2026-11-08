#pragma once
#include <vector>
#include <algorithm>
#include "Keyframe.h"
#include "../Common/Transform.h"


template <typename T>
class ChannelTrack
{
public:
	std::vector<Keyframe<T>> keys_;

	void AddKey(float time, const T& value, InterpolationType interp = InterpolationType::Linear)
	{
		keys_.emplace_back(time, value, interp);
		std::sort(keys_.begin(), keys_.end(), [](const Keyframe<T>& a, const Keyframe<T>& b)
			{
				return a.time_ < b.time_;
			});
	}

	void SetKey(float time, const T& value, InterpolationType interp = InterpolationType::Linear)
	{
		const float epsilon = 0.01f;
		for (auto& k : keys_)
		{
			if (std::abs(k.time_ - time) < epsilon)
			{
				k.value_ = value;
				k.interpolation_ = interp;
				k.isUserEdited_ = true; // ★ 手動変更フラグを立てる
				return;
			}
		}

		// 新規キー追加
		Keyframe<T> newKey(time, value, interp);
		newKey.isUserEdited_ = true; // ★ 新規追加時もフラグを立てる
		keys_.push_back(newKey);

		std::sort(keys_.begin(), keys_.end(), [](const Keyframe<T>& a, const Keyframe<T>& b) {
			return a.time_ < b.time_;
			});
	}

	T Sample(float time, const T& defaultValue) const
	{
		if (keys_.empty()) return defaultValue;
		if (keys_.size() == 1 || time <= keys_.front().time_) return keys_.front().value_;
		if (time >= keys_.back().time_) return keys_.back().value_;

		auto it = std::upper_bound(keys_.begin(), keys_.end(), time,
			[](float t, const Keyframe<T>& key) {
				return t < key.time_;
			});

		size_t nextIdx = std::distance(keys_.begin(), it);
		size_t prevIdx = nextIdx - 1;

		const auto& prevKey = keys_[prevIdx];
		const auto& nextKey = keys_[nextIdx];

		float duration = nextKey.time_ - prevKey.time_;
		float factor = (duration > 0.0f) ? ((time - prevKey.time_) / duration) : 0.0f;

		return KeyframeMath::Interpolate(prevKey.value_, nextKey.value_, factor, prevKey.interpolation_);
	}
};

struct BoneTrack
{
	int boneIndex_ = -1;
	ChannelTrack<DirectX::XMFLOAT3> translationTrack_;
	ChannelTrack<DirectX::XMFLOAT4> rotationTrack_;
	ChannelTrack<DirectX::XMFLOAT3> scaleTrack_;

	Transform Sample(float time, const Transform& bindPose) const
	{
		Transform result;
		result.translation_ = translationTrack_.Sample(time, bindPose.translation_);
		result.rotation_ = rotationTrack_.Sample(time, bindPose.rotation_);
		result.scale_ = scaleTrack_.Sample(time, bindPose.scale_);
		return result;
	}

	// ★ 現在時刻にTRSをキーフレームとして打つ
	void SetKeyframe(float time, const Transform& transform)
	{
		translationTrack_.SetKey(time, transform.translation_);
		rotationTrack_.SetKey(time, transform.rotation_);
		scaleTrack_.SetKey(time, transform.scale_);
	}
};