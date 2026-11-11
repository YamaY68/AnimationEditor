#pragma once
#include<DirectXMath.h>

using namespace DirectX;
struct Transform
{
	XMFLOAT3 translation_ = { 0.0f,0.0f,0.0f };
	XMFLOAT4 rotation_ = { 0.0f,0.0f,0.0f,1.0f };
	XMFLOAT3 scale_ = { 1.0f,1.0f,1.0f };

	Transform(void) = default;
	Transform(const XMFLOAT3& t, const XMFLOAT4& r, const XMFLOAT3& s)
		:translation_(t), rotation_(r), scale_(s) {}


	XMMATRIX ToMatrix(void)const
	{
		XMVECTOR s = XMLoadFloat3(&scale_);
		XMVECTOR r = XMLoadFloat4(&rotation_);
		XMVECTOR t = XMLoadFloat3(&translation_);

		return XMMatrixScalingFromVector(s) *
			XMMatrixRotationQuaternion(r) *
			XMMatrixTranslationFromVector(t);
	}

	/// <summary>
	/// ÇQÇ¬ÇÃTransformä‘Çê¸å`ï‚äÆ
	/// </summary>
	/// <param name="a"></param>
	/// <param name="b"></param>
	/// <param name="t"></param>
	/// <returns></returns>
	static Transform Interpolate(const Transform& a, const Transform& b, float t)
	{
		XMVECTOR tA = XMLoadFloat3(&a.translation_);
		XMVECTOR tB = XMLoadFloat3(&b.translation_);
		XMVECTOR rA = XMLoadFloat4(&a.rotation_);
		XMVECTOR rB = XMLoadFloat4(&b.rotation_);
		XMVECTOR sA = XMLoadFloat3(&a.scale_);
		XMVECTOR sB = XMLoadFloat3(&b.scale_);

		Transform result;
		XMStoreFloat3(&result.translation_, XMVectorLerp(tA, tB, t));
		XMStoreFloat4(&result.rotation_, XMQuaternionSlerp(rA, rB, t));
		XMStoreFloat3(&result.scale_, XMVectorLerp(sA, sB, t));
		return result;
	}
};