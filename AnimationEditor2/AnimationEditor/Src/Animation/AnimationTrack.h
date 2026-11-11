#pragma once
#include<vector>
#include<algorithm>
#include"KeyFrame.h"
#include"../Common/Transform.h"
#include"Easing.h"


template <typename T>
class AnimationTrack
{
public:
	//キーフレームの動的配列
	std::vector<KeyFrame<T >> keys_;

	/// <summary>
	/// キーフレームを追加
	/// </summary>
	/// <param name="time">フレーム時刻</param>
	/// <param name="value">値</param>
	/// <param name="interp">補間タイプ</param>
	/// <param name="isUser">手動編集フラグ</param>
	void AddKey(float time, const T& value, InterpolationType interp = InterpolationType::Linear, bool isUser = false)
	{
		keys_.emplace_back(time, value, interp, isUser);
		std::sort(keys_.begin(), keys_.end(), [](const KeyFrame<T>& a, const KeyFrame<T>& b)
			{
				return a.time_ < b.time_; 
			});
	}

	/// <summary>
	/// 指定時刻のキーフレームを上書き
	/// </summary>
	/// <param name="time">フレーム時刻</param>
	/// <param name="value">格納する値</param>
	/// <param name="interp">補間タイプ</param>
	void SetKey(float time, const T& value, 
		InterpolationType interp = InterpolationType::Linear)
	{
		const float epsilon = 0.001f;
		//既存の中にほぼ一致するものがあるか探索
		for (auto& k : keys_)
		{
			//同じ時刻がある場合は上書き
			if (std::abs(k.time_ - time) < epsilon)
			{
				k.value_ = value;
				k.interpolation_ = interp;
				k.isUserEdited_ = true;
				return;
			}
		}
		AddKey(time, value, interp, true);
	}

	//トラックにキーが存在するか確認する
	bool HasKeys(void)const
	{
		return !keys_.empty();
	}

	/// <summary>
	/// 指定区間の補間キーを設定する
	/// </summary>
	/// <param name="rangeStart">開始時刻</param>
	/// <param name="rangeEnd">終了時刻</param>
	/// <param name="startVal">始点値</param>
	/// <param name="endVal">終点値</param>
	/// <param name="interp">補間タイプ</param>
	void SetRangeKeys(float rangeStart, float rangeEnd, 
		const T& startVal, const T& endVal,
		InterpolationType interp = InterpolationType::Linear)
	{
		const float eps = 0.001f;
		//指定区間の内側にあるキーフレームを全削除
		keys_.erase(std::remove_if(keys_.begin(), keys_.end(), [=](const KeyFrame<T>& k)
			{
				return (k.time_ >= rangeStart - eps && k.time_ <= rangeEnd + eps); 
			}), keys_.end());
		SetKey(rangeStart, startVal, interp);
		SetKey(rangeEnd, endVal, interp);
	}

	/// <summary>
	/// 指定時刻の値をサンプリング
	/// </summary>
	/// <param name="time">サンプリング時刻</param>
	/// <param name="defaultValue">キーがない場合の規定値</param>
	/// <returns>補間された値</returns>
	T Sample(float time, const T& defaultValue)const
	{
		if (keys_.empty())return defaultValue;
		if (keys_.size() == 1 || time <= keys_.front().time_)return keys_.front().value_;
		if (time >= keys_.back().time_)return keys_.back().value_;

		//二分探索でtimeより未来にある最初のキーフレームを探索
		auto it = std::upper_bound(keys_.begin(), keys_.end(), time,
			[](float t, const KeyFrame<T>& key)
			{
				return t < key.time_; 
			});

		size_t nextIdx = std::distance(keys_.begin(), it);
		size_t prevIdx = nextIdx - 1;

		const auto& prevkey = keys_[prevIdx];
		const auto& nextKey = keys_[nextIdx];

		float duration = nextKey.time_ - prevkey.time_;
		float factor = (duration > 0.0f) ? ((time - prevkey.time_) / duration) : 0.0f;

		return KeyFrameMath::Interpolate(prevkey.value_, nextKey.value_, factor, prevkey.interpolation_);
	}
};

struct BoneTrack
{
	int boneIndex = -1;
	AnimationTrack<DirectX::XMFLOAT3>translationTrack_;
	AnimationTrack<DirectX::XMFLOAT4>rotationTrack_;
	AnimationTrack<DirectX::XMFLOAT3>scaleTrack_;

	/// <summary>
	/// 指定時刻のTransformをサンプリング
	/// </summary>
	/// <param name="time">サンプリング時刻</param>
	/// <param name="bindPose">バインド姿勢</param>
	/// <returns>結果のTransform</returns>
	Transform Sample(float time, const Transform& bindPose) const
	{
		Transform result;
		result.translation_ = translationTrack_.Sample(time, bindPose.translation_);
		result.rotation_ = rotationTrack_.Sample(time, bindPose.rotation_);
		result.scale_ = scaleTrack_.Sample(time, bindPose.scale_);
		return result;
	}

	/// <summary>
	/// 現在時刻にTRSをキーフレームとして打つ
	/// </summary>
	/// <param name="time">フレーム時刻</param>
	/// <param name="transform">登録するTransform</param>
	void SetKeyframe(float time, const Transform& transform)
	{
		translationTrack_.SetKey(time, transform.translation_);
		rotationTrack_.SetKey(time, transform.rotation_);
		scaleTrack_.SetKey(time, transform.scale_);
	}
};