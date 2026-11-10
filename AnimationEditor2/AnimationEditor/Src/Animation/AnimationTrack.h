#pragma once
#include<vector>
#include<algorithm>
#include"KeyFrame.h"
#include"../Common/Transform.h"

template <typename T>
class AnimationTrack
{
	//キーフレームの動的配列
	std::vector<KeyFrame<T >> keys_;

	/// <summary>
	/// キーフレームを追加
	/// </summary>
	/// <param name="time">フレーム時刻</param>
	/// <param name="value">値</param>
	/// <param name="interp">補間タイプ</param>
	/// <param name="isUser">手動編集フラグ</param>
	void AddKey(float time, constT& value, InterpolationType interp = InterpolationType::Liniar, bool isUser = false)
	{
		keys_.emplace_back(time, value, interp, isUser);
		std::sort(keys_.begin(), keys_.end(), [](const Keyframe<T>& a, const Keyframe<T>& b))
		{
			return a.time_ < b.time_;
		}
	}

};

