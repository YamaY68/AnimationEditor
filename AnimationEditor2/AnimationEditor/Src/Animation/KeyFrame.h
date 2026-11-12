#pragma once
#include<DirectXMath.h>
#include"Easing.h"

using namespace DirectX;

template<typename T>
struct KeyFrame
{
	float time_ = 0.0f;	//フレーム時刻
	T value_;	//格納する値
	InterpolationType interpolation_ = InterpolationType::Linear;	//補完タイプ
	bool isUserEdited_ = false;	//ユーザー手動編集フラグ

	KeyFrame(void) = default;
	KeyFrame(float t, const T& val, InterpolationType interp = InterpolationType::Linear, bool isUser = false)
		:time_(t), value_(val), interpolation_(interp), isUserEdited_(isUser) {
	}

};
namespace KeyFrameMath
{
	/// <summary>
	/// ３次元ベクトルをイージング適応して補完
	/// </summary>
	/// <param name="a">始点</param>
	/// <param name="b">終点</param>
	/// <param name="t">補間進行度</param>
	/// <param name="type">補間タイプ</param>
	/// <returns>補間後ベクトル</returns>
	inline XMFLOAT3 Interpolate(const XMFLOAT3& a, const XMFLOAT3& b, float t, InterpolationType type)
	{
		float easedT = Easing::Evaluate(t, type);
		XMVECTOR vA = XMLoadFloat3(&a);
		XMVECTOR vB = XMLoadFloat3(&b);
		XMFLOAT3 out;
		XMStoreFloat3(&out, XMVectorLerp(vA, vB, easedT));
		return out;
	}

	/// <summary>
	/// クォータニオンをイージングして球面補間
	/// </summary>
	/// <param name="a">始点クォータニオン</param>
	/// <param name="b">終点クォータニオン</param>
	/// <param name="t">補間進行度</param>
	/// <param name="type">補間タイプ</param>
	/// <returns>補間後クォータニオン</returns>
	inline XMFLOAT4 Interpolate(const XMFLOAT4& a, const XMFLOAT4& b, float t, InterpolationType type)
	{
		float easedT = Easing::Evaluate(t, type);
		XMVECTOR qA = XMLoadFloat4(&a);
		XMVECTOR qB = XMLoadFloat4(&b);
		XMFLOAT4 out;
		XMStoreFloat4(&out, XMQuaternionSlerp(qA, qB, easedT));
		return out;
	}
}