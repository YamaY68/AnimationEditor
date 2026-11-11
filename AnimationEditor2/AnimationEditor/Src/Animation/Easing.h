#pragma once
#include<cmath>
#include<algorithm>

enum class InterpolationType
{
	Linear,	//線形補完
	Step,	//即時切替
	EaseInQuad,	//2次曲線加速
	EaseOutQuad,	//2次曲線加速
	EaseInOutQuad,	//2次曲線加減速
	CubicBezier	//3次ベジェ曲線
};


namespace Easing
{
	inline float Evaluate(float t, InterpolationType type)
	{
		t = std::clamp(t, 0.0f, 1.0f);

		switch (type)
		{
		case InterpolationType::Linear:
			return t;
			break;
		case InterpolationType::Step:
			return 0.0f;
			break;
		case InterpolationType::EaseInQuad:
			return t * t;
			break;
		case InterpolationType::EaseOutQuad:
			return t * (2.0f - t);
			break;
		case InterpolationType::EaseInOutQuad:
			return(t < 0.5f) ? (2.0f * t * t) : (-1.0 + (4.0f - 2.0f * t) * t);
			break;
		case InterpolationType::CubicBezier:
	
			break;
		default:
			break;
		}
	}
}

