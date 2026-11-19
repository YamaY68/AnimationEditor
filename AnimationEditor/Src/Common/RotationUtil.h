#pragma once
#include<DirectXMath.h>
#include<algorithm>
#include<cmath>

/// <summary>
/// 回転(オイラー角とクォータニオン)の変換をまとめたもの
/// UIでは度数法のオイラー角、内部ではクォータニオンを使う
/// </summary>
namespace RotationUtil
{
	/// <summary>
	/// 角度を -180～180度 の範囲に折り返す
	/// </summary>
	inline float WrapDegrees(float deg)
	{
		deg = std::fmod(deg + 180.0f, 360.0f);
		if (deg < 0.0f) deg += 360.0f;
		return deg - 180.0f;
	}

	/// <summary>
	/// オイラー角(度) → クォータニオン
	/// x:X軸回り(Pitch) y:Y軸回り(Yaw) z:Z軸回り(Roll)
	/// 回転の順番はXMQuaternionRotationRollPitchYawに従う(Z→X→Yの順に回す)
	/// </summary>
	inline DirectX::XMFLOAT4 EulerDegToQuaternion(const DirectX::XMFLOAT3& eulerDeg)
	{
		using namespace DirectX;
		XMVECTOR q = XMQuaternionRotationRollPitchYaw(
			XMConvertToRadians(eulerDeg.x),
			XMConvertToRadians(eulerDeg.y),
			XMConvertToRadians(eulerDeg.z));

		XMFLOAT4 out;
		XMStoreFloat4(&out, q);
		return out;
	}

	/// <summary>
	/// クォータニオン → オイラー角(度)
	/// EulerDegToQuaternionの正確な逆変換。Xは -90～90度 の範囲で返る
	/// (同じ向きを表すオイラー角は複数あるため、Xをこの範囲に収める組み合わせを選ぶ)
	/// </summary>
	inline DirectX::XMFLOAT3 QuaternionToEulerDeg(const DirectX::XMFLOAT4& q)
	{
		using namespace DirectX;
		//X軸回り(Pitch)
		float sinp = 2.0f * (q.w * q.x - q.y * q.z);
		float pitch = std::asin(std::clamp(sinp, -1.0f, 1.0f));

		//Y軸回り(Yaw)
		float yaw = std::atan2(2.0f * (q.x * q.z + q.w * q.y), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));

		//Z軸回り(Roll)
		float roll = std::atan2(2.0f * (q.x * q.y + q.w * q.z), 1.0f - 2.0f * (q.x * q.x + q.z * q.z));

		return { XMConvertToDegrees(pitch), XMConvertToDegrees(yaw), XMConvertToDegrees(roll) };
	}

	/// <summary>
	/// 2つのクォータニオンが同じ向きかどうか
	/// qと-qは同じ向きを表すので、内積の絶対値で比べる
	/// </summary>
	/// <param name="threshold">内積の絶対値がこれ以上なら同じ向きとみなす(1に近いほど厳密)</param>
	inline bool IsSameRotation(const DirectX::XMFLOAT4& a, const DirectX::XMFLOAT4& b, float threshold)
	{
		float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
		return std::abs(dot) >= threshold;
	}
}