#pragma once
#include<vector>
#include<algorithm>
#include<cmath>
#include"KeyFrame.h"
#include"../Common/Transform.h"
#include"Easing.h"
#include"../Common/EditorConstants.h"

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
	void AddKey(float time, const T& value, InterpolationType interp = InterpolationType::Linear)
	{
		keys_.emplace_back(time, value, interp);
		std::sort(keys_.begin(), keys_.end(), [](const KeyFrame<T>& a, const KeyFrame<T>& b)
			{
				return a.time_ < b.time_;
			});
	}

	/// <summary>
	/// 指定時刻のキーを探す
	/// </summary>
	/// <returns>見つかったキー(無ければnullptr)</returns>
	const KeyFrame<T>* FindKey(float time)const
	{
		auto it = std::find_if(keys_.begin(), keys_.end(), [time](const KeyFrame<T>& k)
			{
				return std::abs(k.time_ - time) < EditorConst::kKeyTimeEpsilon;
			});
		return (it != keys_.end()) ? &(*it) : nullptr;
	}
	KeyFrame<T>* FindKey(float time)
	{
		//const版と同じ処理なので、const版を呼んで結果のconstを外す
		return const_cast<KeyFrame<T>*>(static_cast<const AnimationTrack&>(*this).FindKey(time));
	}

	/// <summary>
	/// 指定時刻のキーの値を上書きする。無ければ新しく追加する
	/// 既存のキーの補間方法は変えない(オートキーで上書きしたときに補間方法が戻らないように)
	/// </summary>
	/// <param name="interp">新しく追加するときの補間方法</param>
	void SetKey(float time, const T& value, InterpolationType interp = InterpolationType::Linear)
	{
		if (KeyFrame<T>* key = FindKey(time))
		{
			key->value_ = value;
			return;
		}
		AddKey(time, value, interp);
	}

	//指定時刻のキーを削除する(削除したらtrue)
	bool RemoveKey(float time)
	{
		const size_t oldSize = keys_.size();
		keys_.erase(std::remove_if(keys_.begin(), keys_.end(), [time](const KeyFrame<T>& k)
			{
				return std::abs(k.time_ - time) < EditorConst::kKeyTimeEpsilon;
			}), keys_.end());
		return keys_.size() != oldSize;
	}
	//指定時刻のキーの補間方法を取得する(キーが無ければfalse)
	bool GetInterpolation(float time, InterpolationType& out)const
	{
		if (const KeyFrame<T>* key = FindKey(time))
		{
			out = key->interpolation_;
			return true;
		}
		return false;
	}

	//指定時刻のキーの補間方法を設定する(キーが無ければ何もしない)
	void SetInterpolation(float time, InterpolationType type)
	{
		if (KeyFrame<T>* key = FindKey(time))
		{
			key->interpolation_ = type;
		}
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
		const float eps = EditorConst::kKeyTimeEpsilon;		
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
	int boneIndex_ = -1;
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

	//指定時刻にキーがあるか
	bool HasKeyframe(float time)const
	{
		return translationTrack_.FindKey(time) != nullptr
			|| rotationTrack_.FindKey(time) != nullptr
			|| scaleTrack_.FindKey(time) != nullptr;
	}

	//指定時刻のTRSキーを削除
	bool RemoveKeyframe(float time)
	{
		bool removed = translationTrack_.RemoveKey(time);
		removed |= rotationTrack_.RemoveKey(time);
		removed |= scaleTrack_.RemoveKey(time);
		return removed;
	}

	//指定時刻のTRSキーの補間方法を取得
	bool GetInterpolation(float time, InterpolationType& out) const
	{
		return rotationTrack_.GetInterpolation(time, out)
			|| translationTrack_.GetInterpolation(time, out)
			|| scaleTrack_.GetInterpolation(time, out);
	}

	//指定時刻のTRSキーの補間方法をまとめて設定
	void SetInterpolation(float time, InterpolationType type)
	{
		translationTrack_.SetInterpolation(time, type);
		rotationTrack_.SetInterpolation(time, type);
		scaleTrack_.SetInterpolation(time, type);
	}

	/// <summary>
	/// 指定区間の既存のキーを一掃し、始点終点の姿勢で補間置換
	/// </summary>
	/// <param name="rangeStart">始点</param>
	/// <param name="rangeEnd">終点</param>
	/// <param name="startTransform">始点のローカルTransoform</param>
	/// <param name="endTransform">終点のローカルTransform</param>
	void ReplaceRange(float rangeStart, float rangeEnd, const Transform& startTransform, const Transform& endTransform)
	{
		translationTrack_.SetRangeKeys(rangeStart, rangeEnd, startTransform.translation_, endTransform.translation_);
		rotationTrack_.SetRangeKeys(rangeStart, rangeEnd, startTransform.rotation_, endTransform.rotation_);
		scaleTrack_.SetRangeKeys(rangeStart, rangeEnd, startTransform.scale_, endTransform.scale_);
	}
};