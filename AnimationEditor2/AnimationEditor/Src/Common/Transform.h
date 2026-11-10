#pragma once
#include<DirectXMath.h>

using namespace DirectX;
struct Transform
{
	XMFLOAT3 translation = { 0.0f,0.0f,0.0f };
	XMFLOAT4 rotation = { 0.0f,0.0f,0.0f,1.0f };
	XMFLOAT3 scale = { 1.0f,1.0f,1.0f };

	Transform(void) = default;
	Transform(const XMFLOAT3& t, const XMFLOAT4& r, const XMFLOAT3& s)
		:translation(t), rotation(r), scale(s) {}


	XMMATRIX ToMatrix(void)const
	{
		XMVECTOR s = XMLoadFloat3(&scale);
		XMVECTOR r = XMLoadFloat4(&rotation);
		XMVECTOR t = XMLoadFloat3(&translation);

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
		XMVECTOR tA = XMLoadFloat3(&a.translation);
		XMVECTOR tB = XMLoadFloat3(&b.translation);
		XMVECTOR rA = XMLoadFloat4(&a.rotation);
		XMVECTOR rB = XMLoadFloat4(&b.rotation);
		XMVECTOR sA = XMLoadFloat3(&a.scale);
		XMVECTOR sB = XMLoadFloat3(&b.scale);

		Transform result;
		XMStoreFloat3(&result.translation, XMVectorLerp(tA, tB, t));
		XMStoreFloat4(&result.rotation, XMQuaternionSlerp(rA, rB, t));
		XMStoreFloat3(&result.scale, XMVectorLerp(sA, sB, t));
		return result;
	}
};