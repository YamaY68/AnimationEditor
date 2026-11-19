#pragma once
#include<DirectXMath.h>

/// <summary>
/// ボーン1本分の姿勢(移動・回転・スケール)
/// 親ボーンから見たローカルの値として使う
/// </summary>
struct Transform
{
	DirectX::XMFLOAT3 translation_ = { 0.0f,0.0f,0.0f };		//移動
	DirectX::XMFLOAT4 rotation_ = { 0.0f,0.0f,0.0f,1.0f };	//回転(クォータニオン)
	DirectX::XMFLOAT3 scale_ = { 1.0f,1.0f,1.0f };			//スケール

	//初期値は「移動0・回転なし・スケール1」＝何も変化させない姿勢
	Transform(void) = default;
	Transform(const DirectX::XMFLOAT3& t, const DirectX::XMFLOAT4& r, const DirectX::XMFLOAT3& s)
		:translation_(t), rotation_(r), scale_(s) {
	}

	/// <summary>
	/// 4x4行列に変換する
	/// DirectXMathは行ベクトル(左から掛ける)なので、S→R→Tの順に掛ける
	/// </summary>
	DirectX::XMMATRIX ToMatrix(void)const
	{
		using namespace DirectX;
		XMVECTOR s = XMLoadFloat3(&scale_);
		XMVECTOR r = XMLoadFloat4(&rotation_);
		XMVECTOR t = XMLoadFloat3(&translation_);

		return XMMatrixScalingFromVector(s) *
			XMMatrixRotationQuaternion(r) *
			XMMatrixTranslationFromVector(t);
	}

	/// <summary>
	/// 2つの姿勢の間を補間する(移動・スケールは線形、回転は球面線形補間)
	/// </summary>
	/// <param name="a">t=0のときの姿勢</param>
	/// <param name="b">t=1のときの姿勢</param>
	/// <param name="t">補間の割合(0～1)</param>
	static Transform Interpolate(const Transform& a, const Transform& b, float t)
	{
		using namespace DirectX;
		Transform result;
		XMStoreFloat3(&result.translation_,
			XMVectorLerp(XMLoadFloat3(&a.translation_), XMLoadFloat3(&b.translation_), t));
		XMStoreFloat4(&result.rotation_,
			XMQuaternionSlerp(XMLoadFloat4(&a.rotation_), XMLoadFloat4(&b.rotation_), t));
		XMStoreFloat3(&result.scale_,
			XMVectorLerp(XMLoadFloat3(&a.scale_), XMLoadFloat3(&b.scale_), t));
		return result;
	}
};