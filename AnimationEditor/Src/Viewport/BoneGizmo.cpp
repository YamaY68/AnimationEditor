#include "BoneGizmo.h"
#include<DxLib.h>
#include<imgui.h>
#include<ImGuizmo.h>
#include<cstdio>
#include"../Animation/TwoBoneIK.h"

using namespace DirectX;

namespace
{
	//ギズモで動かしたワールド行列を、ボーンのローカル行列に戻す
	//World = Local × ParentWorld なので、Local = World × ParentWorldの逆行列
	XMMATRIX WorldToLocal(const XMFLOAT4X4& world, const Skeleton& skeleton,
		const std::vector<XMFLOAT4X4>& worldMatrices, int boneIndex)
	{
		XMMATRIX local = XMLoadFloat4x4(&world);
		const int parent = skeleton.GetBone(boneIndex).parentIndex_;
		if (parent >= 0)
		{
			const XMMATRIX parentWorld = XMLoadFloat4x4(&worldMatrices[parent]);
			local = XMMatrixMultiply(local, XMMatrixInverse(nullptr, parentWorld));
		}
		return local;
	}

	//行列から回転だけを取り出す
	XMFLOAT4 GetRotation(const XMFLOAT4X4& m)
	{
		XMFLOAT4 out = { 0.0f, 0.0f, 0.0f, 1.0f };
		XMVECTOR s, r, t;
		if (XMMatrixDecompose(&s, &r, &t, XMLoadFloat4x4(&m)))
		{
			XMStoreFloat4(&out, XMQuaternionNormalize(r));
		}
		return out;
	}
}

std::vector<int> BoneGizmo::Draw(const Skeleton& skeleton, const std::vector<XMFLOAT4X4>& worldMatrices,
	Pose& inOutPose, int boneIndex)
{
	std::vector<int> editedBones;

	drawn_ = false;
	DrawStatus();
	if (boneIndex < 0
		|| boneIndex >= static_cast<int>(worldMatrices.size())
		|| boneIndex >= static_cast<int>(inOutPose.GetBoneCount()))
	{
		return editedBones;
	}

	//IKで、選んだボーンからチェーンが作れないときは、ギズモを出さずに理由を表示する
	if (operation_ == Operation::IK)
	{
		int rootBone = -1;
		int midBone = -1;
		if (!TwoBoneIK::FindChain(skeleton, boneIndex, rootBone, midBone))
		{
			const ImVec2 pos(10.0f, ImGui::GetFrameHeight() + 26.0f);
			ImGui::GetBackgroundDrawList()->AddText(pos, IM_COL32(255, 120, 120, 255),
				"IK: Select an end bone (hand, foot, etc.)");
			return editedBones;
		}
	}

	//画面全体を対象にし、3Dの上・ImGuiのウィンドウの下に描く
	const ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetDrawlist(ImGui::GetBackgroundDrawList());
	ImGuizmo::SetRect(0.0f, 0.0f, io.DisplaySize.x, io.DisplaySize.y);

	//ImGuiのウィンドウの上や、スライダーなどを操作中のときはギズモを使わない
	//(io.WantCaptureMouseはImGuizmo自身が軸に乗ったときにもtrueにするので、判定には使わない)
	const bool overUI = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) || ImGui::IsAnyItemActive();
	ImGuizmo::Enable(ImGuizmo::IsUsing() || !overUI);

	//DxLibのカメラ行列(DirectXMathと同じ並びで、4行目が平行移動)
	const MATRIX view = GetCameraViewMatrix();
	const MATRIX proj = GetCameraProjectionMatrix();

	XMFLOAT4X4 world = worldMatrices[boneIndex];

	//IKは、自分で持っている目標の位置と向きでギズモを出す
	//(ドラッグしていない間は、先端のボーンに合わせ直す)
	if (operation_ == Operation::IK)
	{
		if (!ImGuizmo::IsUsing())
		{
			ikTargetPosition_ = { world._41, world._42, world._43 };
			ikTargetRotation_ = GetRotation(world);
		}
		const XMMATRIX target = XMMatrixRotationQuaternion(XMLoadFloat4(&ikTargetRotation_))
			* XMMatrixTranslation(ikTargetPosition_.x, ikTargetPosition_.y, ikTargetPosition_.z);
		XMStoreFloat4x4(&world, target);
	}

	//今のモードをImGuizmoの種類に変換する
	const bool rotating = (operation_ == Operation::Rotate) || (operation_ == Operation::IK && ikRotate_);
	const ImGuizmo::OPERATION op = rotating ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE;
	//IKの移動は、ワールドの軸で動かす(それ以外は設定に従う)
	const bool ikMove = (operation_ == Operation::IK && !ikRotate_);
	const bool useLocal = (space_ == Space::Local) && !ikMove;
	const ImGuizmo::MODE mode = useLocal ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

	drawn_ = true;
	if (!ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], op, mode, &world.m[0][0]))
	{
		return editedBones;
	}

	switch (operation_)
	{
	case Operation::Translate:
	{
		//移動なら、平行移動だけを書き換える
		//(回転・スケールまで行列から分解し直すと、計算の誤差が少しずつたまるため)
		XMFLOAT4X4 l;
		XMStoreFloat4x4(&l, WorldToLocal(world, skeleton, worldMatrices, boneIndex));
		inOutPose[boneIndex].translation_ = { l._41, l._42, l._43 };
		editedBones.push_back(boneIndex);
		break;
	}
	case Operation::Rotate:
	{
		//回転なら、ローカル行列を分解して回転だけを書き換える(移動・スケールはそのまま)
		XMVECTOR s, r, t;
		if (XMMatrixDecompose(&s, &r, &t, WorldToLocal(world, skeleton, worldMatrices, boneIndex)))
		{
			XMStoreFloat4(&inOutPose[boneIndex].rotation_, XMQuaternionNormalize(r));
			editedBones.push_back(boneIndex);
		}
		break;
	}
	case Operation::IK:
	{
		//目標の位置と向きを更新し、先端が届くように解き直す
		//(IK移動では向きは変わらず、IK回転では位置は変わらない)
		ikTargetPosition_ = { world._41, world._42, world._43 };
		ikTargetRotation_ = GetRotation(world);

		int rootBone = -1;
		int midBone = -1;
		if (TwoBoneIK::Solve(skeleton, worldMatrices, boneIndex,
			ikTargetPosition_, ikTargetRotation_, inOutPose, rootBone, midBone))
		{
			editedBones.push_back(rootBone);
			editedBones.push_back(midBone);
			editedBones.push_back(boneIndex);	//先端の向きも書き換わる
		}
		break;
	}
	}
	return editedBones;
}

void BoneGizmo::SetOperation(Operation op)
{
	//IKに入るときは、位置を動かすところから始める
	if (op == Operation::IK && operation_ != Operation::IK)
	{
		ikRotate_ = false;
	}
	operation_ = op;
}

void BoneGizmo::ToggleOperation(void)
{
	switch (operation_)
	{
	case Operation::Translate: operation_ = Operation::Rotate;    break;
	case Operation::Rotate:    operation_ = Operation::Translate; break;
	case Operation::IK:        ikRotate_ = !ikRotate_;            break;	//IKの中で移動と回転を切り替える
	}
}

bool BoneGizmo::IsHovered(void) const
{
	return drawn_ && ImGuizmo::IsOver();
}

bool BoneGizmo::IsUsing(void) const
{
	return drawn_ && ImGuizmo::IsUsing();
}

void BoneGizmo::DrawStatus(void) const
{
	const char* opName = "Move";
	if (operation_ == Operation::Rotate) opName = "Rotate";
	if (operation_ == Operation::IK)     opName = ikRotate_ ? "IK Rotate" : "IK Move";
	const char* spaceName = (space_ == Space::Local) ? "Local" : "World";

	char text[192];
	std::snprintf(text, sizeof(text),
		"Gizmo: %s / %s   (W:Move  E:Rotate  I:IK  X:Local/World  RightClick:Switch)", opName, spaceName);

	//メニューバーのすぐ下に表示する
	const ImVec2 pos(10.0f, ImGui::GetFrameHeight() + 8.0f);
	ImGui::GetBackgroundDrawList()->AddText(pos, IM_COL32(255, 255, 255, 200), text);
}