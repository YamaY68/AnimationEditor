#pragma once
#include<algorithm>

/// <summary>
/// キーから次のキーまでの補間方法
/// </summary>
enum class InterpolationType
{
	Linear,		//一定の速さで変化する
	Step,		//次のキーまで値を保ち、キーの位置で切り替わる
	EaseIn,		//ゆっくり動き出す(2次曲線)
	EaseOut,	//ゆっくり止まる(2次曲線)
	EaseInOut,	//ゆっくり動き出して、ゆっくり止まる
};

namespace Easing
{
	/// <summary>
	/// 補間の進み具合を、補間方法に応じた曲線で変換する
	/// </summary>
	/// <param name="t">キー間の進み具合(0:前のキー ～ 1:次のキー)</param>
	/// <returns>変換後の進み具合(0～1)</returns>
	inline float Evaluate(float t, InterpolationType type)
	{
		t = std::clamp(t, 0.0f, 1.0f);

		switch (type)
		{
		case InterpolationType::Step:
			return 0.0f;
		case InterpolationType::EaseIn:
			return t * t;
		case InterpolationType::EaseOut:
			return t * (2.0f - t);
		case InterpolationType::EaseInOut:
			return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
		case InterpolationType::Linear:
		default:
			return t;
		}
	}
}