#pragma once
#include<DirectXMath.h>
#include"Easing.h"


/// <summary>
/// ある時刻の値1つ分(キーフレーム)
/// </summary>
/// <typeparam name="T">値の型(移動・スケールならXMFLOAT3、回転ならXMFLOAT4)</typeparam>
template<typename T>
struct KeyFrame
{
	float time_ = 0.0f;	//時刻(秒)
	T value_{};			//その時刻の値
	InterpolationType interpolation_ = InterpolationType::Linear;	//このキーから次のキーまでの補間方法

	KeyFrame(void) = default;
	KeyFrame(float t, const T& val, InterpolationType interp = InterpolationType::Linear)
		:time_(t), value_(val), interpolation_(interp) {
	}
};
/// <summary>
/// キー同士の値を補間する関数(値の型ごとに用意する)
/// </summary>
namespace KeyFrameMath
{
	/// <summary>
	/// 3次元ベクトル(移動・スケール)を線形補間する
	/// </summary>
	/// <param name="t">キー間の進み具合(0～1)。補間方法の曲線はこの中でかける</param>
	inline DirectX::XMFLOAT3 Interpolate(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b, float t, InterpolationType type)
	{
		using namespace DirectX;
		float easedT = Easing::Evaluate(t, type);
		XMFLOAT3 out;
		XMStoreFloat3(&out, XMVectorLerp(XMLoadFloat3(&a), XMLoadFloat3(&b), easedT));
		return out;
	}

	/// <summary>
	/// クォータニオン(回転)を球面線形補間する
	/// </summary>
	inline DirectX::XMFLOAT4 Interpolate(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float t, InterpolationType type)
	{
		using namespace DirectX;
		float easedT = Easing::Evaluate(t, type);
		XMFLOAT4 out;
		XMStoreFloat4(&out, XMQuaternionSlerp(XMLoadFloat4(&a), XMLoadFloat4(&b), easedT));
		return out;
	}
}