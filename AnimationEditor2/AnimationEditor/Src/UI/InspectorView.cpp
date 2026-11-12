#include "InspectorView.h"
#include<imgui.h>
#include<DirectXMath.h>
#include<cmath>
using namespace DirectX;
static XMFLOAT3 QuaternionToEuler(const XMFLOAT4& q)
{
	//Z軸回りの回転角をアークタンジェント(Roll)
	float sinr_cosp = 2.0f * (q.w * q.x + q.y * q.z);
	float cosr_cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	float roll = std::atan2(sinr_cosp, cosr_cosp);

	//X軸回りの回転角のアークサイン(Pitch)
	float sinp = 2.0f * (q.w * q.y - q.z * q.x);
	float pitch = (std::abs(sinp) >= 1.0f) ? std::copysign(XM_PIDIV2, sinp) : std::asin(sinp);

	//Y軸回りの回転角をアークタンジェント(Yaw)
	float siny_cosp = 2.0f * (q.w * q.z + q.x * q.y);
	float cosy_cosp = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	float yaw = std::atan2(siny_cosp, cosy_cosp);

	//ラジアンから度数法へ変換
	return { XMConvertToDegrees(pitch), XMConvertToDegrees(yaw), XMConvertToDegrees(roll) };
}

static XMFLOAT4 EulerToQuaternion(const XMFLOAT3& euler)
{
	//度数法からDirectXMathが要求するラジアンへ変換
	float p = XMConvertToRadians(euler.x);
	float y = XMConvertToRadians(euler.y);
	float r = XMConvertToRadians(euler.z);

	//Pitch->Yaw->Rollの淳で回転クォータニオンを構築
	XMVECTOR q = XMQuaternionRotationRollPitchYaw(p, y, r);

	//SIMDレジスタからメモリ構造体へ書き出して返却
	XMFLOAT4 out;
	XMStoreFloat4(&out, q);
	return out;
}

bool InspectorView::Draw(const Skeleton& skeleton, Pose& inOutPose, AnimationClip* currentClip, float currentTime)
{
	//このフレームでキーフレームの追加、更新が行われたか記録するフラグ
	bool keyEdited = false;

	ImGui::SetNextWindowSize(ImVec2(340, 560), ImGuiCond_FirstUseEver);
	ImGui::Begin("Inspector");

	//スケルトンがまだ読み込まれていない場合のガード表示
	if (skeleton.GetBoneCount() == 0)
	{
		ImGui::Text("No Skeleton Loaded");
		ImGui::End();
		return false;
	}

	//親モデルの差し替え等でボーン数が減り、選択インデックスが範囲外になった場合は未設定に戻す

}

void InspectorView::DrawBoneNode(int boneIndex, const Skeleton& skeleton)
{
}
