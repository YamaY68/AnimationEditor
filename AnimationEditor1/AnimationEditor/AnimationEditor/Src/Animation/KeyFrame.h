#pragma once
#include <DirectXMath.h>
#include "Easing.h"

template<typename T>
struct Keyframe
{
    float time_ = 0.0f;
    T value_;
    InterpolationType interpolation_ = InterpolationType::Linear;
    bool isUserEdited_ = false; 

    Keyframe(void) = default;
    Keyframe(float t, const T& val, InterpolationType interp = InterpolationType::Linear, bool isUserEdited = false)
        : time_(t), value_(val), interpolation_(interp), isUserEdited_(isUserEdited) {
    }
};

namespace KeyframeMath
{
    inline DirectX::XMFLOAT3 Interpolate(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b,
        float t, InterpolationType type)
    {
        float easedT = Easing::Evaluate(t, type);
        using namespace DirectX;
        XMVECTOR vA = XMLoadFloat3(&a);
        XMVECTOR vB = XMLoadFloat3(&b);
        XMVECTOR result = XMVectorLerp(vA, vB, easedT);
        XMFLOAT3 out;
        XMStoreFloat3(&out, result);
        return out;
    }

    inline DirectX::XMFLOAT4 Interpolate(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b,
        float t, InterpolationType type)
    {
        float easedT = Easing::Evaluate(t, type);
        using namespace DirectX;
        XMVECTOR qA = XMLoadFloat4(&a);
        XMVECTOR qB = XMLoadFloat4(&b);
        XMVECTOR result = XMQuaternionSlerp(qA, qB, easedT);
        XMFLOAT4 out;
        XMStoreFloat4(&out, result);
        return out;
    }
}