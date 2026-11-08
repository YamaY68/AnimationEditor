#pragma once
#include<cmath>
#include<algorithm>

enum class InterpolationType
{
    Linear,         // 線形補間
    Step,           // 即時切り替え
    EaseInQuad,     // 加速
    EaseOutQuad,    // 減速
    EaseInOutQuad,  // 加速・減速
    CubicBezier     // ベジェ曲線
};

namespace Easing
{
    inline float Evaluate(float t, InterpolationType type)
    {
        switch (type) {
        case InterpolationType::Step:
            return 0.0f;
        case InterpolationType::Linear:
            return t;
        case InterpolationType::EaseInQuad:
            return t * t;
        case InterpolationType::EaseOutQuad:
            return t * (2.0f - t);
        case InterpolationType::EaseInOutQuad:
            return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
        default:
            return t;
        }
    }
}