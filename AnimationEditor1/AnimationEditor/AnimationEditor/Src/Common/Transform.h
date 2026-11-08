#pragma once
#include<DirectXMath.h>

struct Transform
{
	DirectX::XMFLOAT3 translation_ = { 0.0f,0.0f,0.0f };
	DirectX::XMFLOAT4 rotation_ = { 0.0f,0.0f,0.0f,1.0f };
	DirectX::XMFLOAT3 scale_ = { 1.0f,1.0f,1.0f };

	Transform(void) = default;
	Transform(const DirectX::XMFLOAT3& t, const DirectX::XMFLOAT4 r, const DirectX::XMFLOAT3& s)
		:translation_(t), rotation_(r), scale_(s) {
	}

	//TRSを行列に変換する
	DirectX::XMMATRIX ToMatrix(void)const
	{
		using namespace DirectX;
		XMVECTOR s = XMLoadFloat3(&scale_);
		XMVECTOR r = XMLoadFloat4(&rotation_);
		XMVECTOR t = XMLoadFloat3(&translation_);

		//スケール、回転、平行移動の順番
		return	XMMatrixScalingFromVector(s) *
				XMMatrixRotationQuaternion(r) * 
				XMMatrixTranslationFromVector(t);
	}
	static Transform Multiply(const Transform& parent, const Transform& child)
	{
		using namespace DirectX;

		XMVECTOR pT = XMLoadFloat3(&parent.translation_);
		XMVECTOR pR = XMLoadFloat4(&parent.rotation_);
		XMVECTOR pS = XMLoadFloat3(&parent.scale_);

		XMVECTOR cT = XMLoadFloat3(&child.translation_);
		XMVECTOR cR = XMLoadFloat4(&child.rotation_);
		XMVECTOR cS = XMLoadFloat3(&child.scale_);

		// 新しいスケール = 親スケール * 子スケール
		XMVECTOR outS = XMVectorMultiply(pS, cS);

		// 新しい回転 = 親回転 * 子回転
		XMVECTOR outR = XMQuaternionMultiply(pR, cR);

		// 新しい平行移動 = 親回転 * (親スケール * 子平行移動) + 親平行移動
		XMVECTOR rotatedChildT = XMVector3Rotate(XMVectorMultiply(pS, cT), pR);
		XMVECTOR outT = XMVectorAdd(pT, rotatedChildT);

		Transform result;
		XMStoreFloat3(&result.translation_, outT);
		XMStoreFloat4(&result.rotation_, outR);
		XMStoreFloat3(&result.scale_, outS);
		return result;
	}

	static Transform Interpolate(const Transform& a, const Transform& b, float t)
	{
		using namespace DirectX;

		XMVECTOR tA = XMLoadFloat3(&a.translation_);
		XMVECTOR tB = XMLoadFloat3(&b.translation_);
		XMVECTOR rA = XMLoadFloat4(&a.rotation_);
		XMVECTOR rB = XMLoadFloat4(&b.rotation_);
		XMVECTOR sA = XMLoadFloat3(&a.scale_);
		XMVECTOR sB = XMLoadFloat3(&b.scale_);

		// 平行移動・スケールは Lerp
		XMVECTOR outT = XMVectorLerp(tA, tB, t);
		XMVECTOR outS = XMVectorLerp(sA, sB, t);

		// 回転は球面線形補間（Slerp）
		XMVECTOR outR = XMQuaternionSlerp(rA, rB, t);

		Transform result;
		XMStoreFloat3(&result.translation_, outT);
		XMStoreFloat4(&result.rotation_, outR);
		XMStoreFloat3(&result.scale_, outS);
		return result;
	}

};