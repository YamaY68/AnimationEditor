#include "InspectorView.h"
#include<imgui.h>
#include<algorithm>
#include<vector>
#include"../Common/EditorConstants.h"
#include"../Common/RotationUtil.h"

namespace
{
	//表示中のオイラー角を使い続けるかの判定で、この内積以上なら「回転は外から変わっていない」とみなす
	//(オートキーで元＋修正分から計算し直したときの、ごくわずかな誤差は無視するため)
	constexpr float kSameRotationThreshold = 0.99999f;

	constexpr ImVec2 kWindowSize(340.0f, 560.0f);	//ウィンドウの初期サイズ
	constexpr float kHierarchyHeight = 220.0f;		//ボーン階層ツリーの高さ
	constexpr ImVec4 kReadyColor(0.4f, 1.0f, 0.4f, 1.0f);	//In点が記録済みのときの文字色

	//補間方法の選択肢(InterpolationTypeと同じ順番に並べる)
	const char* const kInterpNames[] = { "Linear", "Step", "EaseIn", "EaseOut", "EaseInOut" };
}

InspectorResult InspectorView::Draw(const Skeleton& skeleton, Pose& inOutPose, EditorState& state)
{
	InspectorResult result;

	ImGui::SetNextWindowSize(kWindowSize, ImGuiCond_FirstUseEver);
	ImGui::Begin("Inspector");

	if (skeleton.GetBoneCount() == 0)
	{
		ImGui::Text("No Skeleton Loaded");
		ImGui::End();
		return result;
	}

	//モデルの差し替えでボーン数が減り、選択中のボーンが範囲外になったら選択を外す
	if (state.selectedBoneIndex >= static_cast<int>(skeleton.GetBoneCount()))
	{
		state.selectedBoneIndex = -1;
	}

	DrawBoneHierarchy(skeleton, state);

	const int boneIndex = state.selectedBoneIndex;
	if (boneIndex < 0 || boneIndex >= static_cast<int>(inOutPose.GetBoneCount()))
	{
		ImGui::TextDisabled("Select a bone from hierarchy to edit");
	}
	else
	{
		ImGui::Text("Selected:%s(ID:%d)", skeleton.GetBone(boneIndex).name_.c_str(), boneIndex);

		Transform& trs = inOutPose[boneIndex];
		DrawTransformEditor(trs, boneIndex, skeleton, state, result);
		ImGui::Separator();

		if (AnimationClip* clip = state.GetActiveClip())
		{
			DrawKeyControls(trs, boneIndex, skeleton, *clip, state, result);
			ImGui::Spacing();
			DrawRangeReplace(trs, boneIndex, skeleton, *clip, state, result);
		}
		else
		{
			ImGui::TextDisabled("Load an Animation clip to edit keys");
		}
	}

	//ドラッグが終わったら、次の編集で新しくUndoの履歴を積めるようにする
	if (!ImGui::IsAnyItemActive())
	{
		editRecorded_ = false;
	}

	ImGui::End();
	return result;
}

//=============================================================
// ボーンの階層ツリー
//=============================================================

void InspectorView::DrawBoneHierarchy(const Skeleton& skeleton, EditorState& state)
{
	ImGui::BeginChild("BoneHierarchy", ImVec2(0, kHierarchyHeight), true);
	ImGui::Text("Skeleton Hierarchy(%zu bones)", skeleton.GetBoneCount());
	ImGui::Separator();

	//親がいないボーン(ルート)から、子をたどってツリーを表示する
	for (size_t i = 0; i < skeleton.GetBoneCount(); ++i)
	{
		if (skeleton.GetBone(i).parentIndex_ == -1)
		{
			DrawBoneNode(static_cast<int>(i), skeleton, state);
		}
	}
	ImGui::EndChild();
	ImGui::Separator();
}

void InspectorView::DrawBoneNode(int boneIndex, const Skeleton& skeleton, EditorState& state)
{
	//このボーンの子を集める
	std::vector<int> children;
	for (size_t i = 0; i < skeleton.GetBoneCount(); ++i)
	{
		if (skeleton.GetBone(i).parentIndex_ == boneIndex)
		{
			children.push_back(static_cast<int>(i));
		}
	}

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
	if (state.selectedBoneIndex == boneIndex)
	{
		flags |= ImGuiTreeNodeFlags_Selected;	//選択中は青くハイライト
	}
	if (children.empty())
	{
		//子がいないボーンは矢印を出さない
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	//ボーン番号をImGuiのIDにする(同じ名前のボーンがあってもIDが重ならないように)
	const bool nodeOpen = ImGui::TreeNodeEx(
		reinterpret_cast<void*>(static_cast<intptr_t>(boneIndex)),
		flags, "%s", skeleton.GetBone(boneIndex).name_.c_str());

	if (ImGui::IsItemClicked())
	{
		state.selectedBoneIndex = boneIndex;
	}

	//開いていれば子を表示する
	if (!children.empty() && nodeOpen)
	{
		for (int child : children)
		{
			DrawBoneNode(child, skeleton, state);
		}
		ImGui::TreePop();
	}
}

//=============================================================
// 姿勢の編集
//=============================================================

void InspectorView::OnPoseChanged(const Transform& trs, int boneIndex, const Skeleton& skeleton,
	EditorState& state, InspectorResult& result)
{
	result.poseEdited = true;

	AnimationClip* clip = state.GetActiveClip();
	if (!state.autoKey || clip == nullptr) return;

	//ドラッグ1回につき、Undoの履歴は1つだけ積む
	if (!editRecorded_)
	{
		state.RecordUndo();
		editRecorded_ = true;
	}
	clip->SetOffsetKey(boneIndex, state.playback.time, trs, skeleton);
	result.keyEdited = true;
}

void InspectorView::DrawTransformEditor(Transform& trs, int boneIndex, const Skeleton& skeleton,
	EditorState& state, InspectorResult& result)
{
	//位置
	float t[3] = { trs.translation_.x, trs.translation_.y, trs.translation_.z };
	if (ImGui::DragFloat3("Position", t, 0.1f))
	{
		trs.translation_ = { t[0], t[1], t[2] };
		OnPoseChanged(trs, boneIndex, skeleton, state, result);
	}

	//回転
	//毎フレームクォータニオンから計算し直すと、Xが±90度で跳ね返ってしまうので、
	//表示中のオイラー角を覚えておき、回転が外から変わったときだけ計算し直す
	if (eulerCacheBone_ != boneIndex
		|| !RotationUtil::IsSameRotation(trs.rotation_, eulerCacheQuat_, kSameRotationThreshold))
	{
		eulerCache_ = RotationUtil::QuaternionToEulerDeg(trs.rotation_);
		eulerCacheQuat_ = trs.rotation_;
		eulerCacheBone_ = boneIndex;
	}

	float r[3] = { eulerCache_.x, eulerCache_.y, eulerCache_.z };
	if (ImGui::DragFloat3("Rotation", r, 0.5f))
	{
		//表示は -180～180度 に折り返す(360度回しても向きは同じ)
		eulerCache_ = {
			RotationUtil::WrapDegrees(r[0]),
			RotationUtil::WrapDegrees(r[1]),
			RotationUtil::WrapDegrees(r[2]) };
		trs.rotation_ = RotationUtil::EulerDegToQuaternion(eulerCache_);
		eulerCacheQuat_ = trs.rotation_;
		OnPoseChanged(trs, boneIndex, skeleton, state, result);
	}

	//スケール
	float s[3] = { trs.scale_.x, trs.scale_.y, trs.scale_.z };
	if (ImGui::DragFloat3("Scale", s, 0.01f, 0.01f, 10.0f))
	{
		trs.scale_ = { s[0], s[1], s[2] };
		OnPoseChanged(trs, boneIndex, skeleton, state, result);
	}

	//元の姿勢に戻す(クリップがあれば元の動きの姿勢、無ければバインドポーズ)
	if (ImGui::Button("Reset", ImVec2(-1, 24)))
	{
		const AnimationClip* clip = state.GetActiveClip();
		trs = clip
			? clip->SampleBaseBone(boneIndex, state.playback.time, skeleton)
			: skeleton.GetBone(boneIndex).bindPoseLocal_;
		OnPoseChanged(trs, boneIndex, skeleton, state, result);
	}
}

//=============================================================
// キーの操作
//=============================================================

void InspectorView::DrawKeyControls(const Transform& trs, int boneIndex, const Skeleton& skeleton,
	AnimationClip& clip, EditorState& state, InspectorResult& result)
{
	const float time = state.playback.time;
	//文字入力中はショートカットを無効にする
	const bool canShortcut = !ImGui::GetIO().WantTextInput;

	//キーを打つ(ボタン or Kキー)
	if (ImGui::Button("Key Selected Bone (K)", ImVec2(-1, 28))
		|| (canShortcut && ImGui::IsKeyPressed(ImGuiKey_K, false)))
	{
		state.RecordUndo();
		clip.SetOffsetKey(boneIndex, time, trs, skeleton);
		result.keyEdited = true;
	}

	//再生位置のキーを消す(ボタン or Deleteキー)
	if (ImGui::Button("Delete Key (Del)", ImVec2(-1, 24))
		|| (canShortcut && ImGui::IsKeyPressed(ImGuiKey_Delete, false)))
	{
		//キーが無いときに履歴を積まないよう、先に確認する
		if (clip.HasOffsetKey(boneIndex, time))
		{
			state.RecordUndo();
			clip.RemoveOffsetKey(boneIndex, time);
			result.keyEdited = true;
		}
	}

	//再生位置のキーの補間方法(このキーから次のキーまでの動き方)
	InterpolationType interp;
	if (clip.GetOffsetInterpolation(boneIndex, time, interp))
	{
		int current = static_cast<int>(interp);
		if (current < 0 || current >= IM_ARRAYSIZE(kInterpNames)) current = 0;

		ImGui::Text("Interpolation (to next key):");
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::Combo("##Interp", &current, kInterpNames, IM_ARRAYSIZE(kInterpNames)))
		{
			state.RecordUndo();
			clip.SetOffsetInterpolation(boneIndex, time, static_cast<InterpolationType>(current));
			result.keyEdited = true;
		}
	}
	else
	{
		ImGui::TextDisabled("Interpolation: no key at current time");
	}

	//今の時刻の前後に修正0のキーを打ち、修正の影響範囲を限定する
	ImGui::SetNextItemWidth(120.0f);
	ImGui::DragFloat("##LimitRange", &limitRangeSec_, 0.01f, 0.01f, 5.0f, "+-%.2f sec");
	ImGui::SameLine();
	if (ImGui::Button("Limit Edit Range", ImVec2(-1, 0)))
	{
		state.RecordUndo();

		//前後の時刻(クリップの範囲内に収める)
		const float duration = clip.duration_;
		const float before = std::max(0.0f, time - limitRangeSec_);
		const float after = (duration > 0.0f) ? std::min(duration, time + limitRangeSec_) : time + limitRangeSec_;

		//前後の0キーが今の時刻と重なる場合(クリップの端にいるとき)は打たない
		const float eps = EditorConst::kKeyTimeEpsilon;
		if (before < time - eps) clip.SetZeroOffsetKey(boneIndex, before);
		if (after > time + eps)  clip.SetZeroOffsetKey(boneIndex, after);
		result.keyEdited = true;
	}
}

void InspectorView::DrawRangeReplace(const Transform& trs, int boneIndex, const Skeleton& skeleton,
	AnimationClip& clip, EditorState& state, InspectorResult& result)
{
	const float time = state.playback.time;
	ImGui::Text("Range Replacement(In-Out):");

	//今の時刻と姿勢をIn点として記録する
	if (ImGui::Button("Set In Point", ImVec2(-1, 24)))
	{
		rangeStartTime_ = time;
		rangeStartTrs_ = trs;
		rangeStartBone_ = boneIndex;
	}

	//In点は、記録したボーンを選んでいるときだけ有効
	const bool hasInPoint = (rangeStartBone_ == boneIndex);
	if (hasInPoint)
	{
		ImGui::TextColored(kReadyColor, "In time:%.3f sec(Ready)", rangeStartTime_);
	}
	else
	{
		ImGui::TextDisabled("In Time:Not Set");
	}

	//今の時刻をOut点にして、In点～Out点の修正分を置き換える
	if (ImGui::Button("Set Out&Replace Range", ImVec2(-1, 28)))
	{
		//In点が未設定なら、今の姿勢を始点の姿勢にする
		const Transform startPose = hasInPoint ? rangeStartTrs_ : trs;

		state.RecordUndo();
		clip.ReplaceOffsetRange(boneIndex, rangeStartTime_, time, startPose, trs, skeleton);

		rangeStartBone_ = -1;	//次の区間に備えてIn点を消す
		result.keyEdited = true;
	}
}