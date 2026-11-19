#include "InspectorView.h"
#include<imgui.h>
#include<DirectXMath.h>
#include<algorithm>
#include<cmath>
#include"../Edit/UndoStack.h"

using namespace DirectX;

//角度を -180?180度 の範囲に折り返す
static float WrapDegrees(float deg)
{
	deg = std::fmod(deg + 180.0f, 360.0f);
	if (deg < 0.0f) deg += 360.0f;
	return deg - 180.0f;
}
static XMFLOAT3 QuaternionToEuler(const XMFLOAT4& q)
{
	//X軸回りの回転角(Pitch)
	float sinp = 2.0f * (q.w * q.x - q.y * q.z);
	float pitch = std::asin(std::clamp(sinp, -1.0f, 1.0f));

	//Y軸回りの回転角(Yaw)
	float yaw = std::atan2(2.0f * (q.x * q.z + q.w * q.y), 1.0f - 2.0f * (q.x * q.x + q.y * q.y));

	//Z軸回りの回転角(Roll)
	float roll = std::atan2(2.0f * (q.x * q.y + q.w * q.z), 1.0f - 2.0f * (q.x * q.x + q.z * q.z));

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

bool InspectorView::Draw(const Skeleton& skeleton, Pose& inOutPose, AnimationClip* currentClip, float currentTime,
	UndoStack& undoStack, int clipIndex, bool autoKey, bool& outPoseEdited)
{
	//このフレームでキーフレームの追加、更新が行われたか記録するフラグ
	bool keyEdited = false;
	outPoseEdited = false;

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
	if (selectedBoneIndex_ >= static_cast<int>(skeleton.GetBoneCount()))
	{
		selectedBoneIndex_ = -1;
		hasRangeStartPose_ = false;
	}

	//ボーン階層ツリー表示
	ImGui::BeginChild("BoneHierarchy", ImVec2(0, 220), true);
	//読み込まれているボーンの総数を表示
	ImGui::Text("Skeleton Hierarchy(%zu bones)", skeleton.GetBoneCount());
	ImGui::Separator();

	//全ボーンを走査し、親を持たないルートボーンから再帰的にツリーを展開
	for (size_t i = 0; i < skeleton.GetBoneCount(); ++i)
	{
		if (skeleton.GetBone(i).parentIndex_ == -1)
		{
			DrawBoneNode(static_cast<int>(i), skeleton);
		}
	}
	ImGui::EndChild();
	ImGui::Separator();

	//選択中ボーンのトランスフォーム編集エリア
	if (selectedBoneIndex_ >= 0 && selectedBoneIndex_ < static_cast<int>(skeleton.GetBoneCount()))
	{
		//選択中のボーン情報
		const auto& bone = skeleton.GetBone(selectedBoneIndex_);
		ImGui::Text("Selected:%s(ID:%d)", bone.name_.c_str(), selectedBoneIndex_);

		//現在姿勢の配列範囲内であることを確認
		if (selectedBoneIndex_ < static_cast<int>(inOutPose.GetBoneCoount()))
		{
			//選択ボーンのローカルTransformへ参照を取得
			Transform& trs = inOutPose[selectedBoneIndex_];

			//姿勢が変更されたときの共通処理(オートキー)
			auto onPoseChanged = [&]()
				{
					outPoseEdited = true;
					if (autoKey && currentClip != nullptr)
					{
						//ドラッグ1回につきUndo履歴は1つだけ積む
						if (!editRecorded_)
						{
							undoStack.Record(clipIndex, *currentClip);
							editRecorded_ = true;
						}
						currentClip->SetOffsetKey(selectedBoneIndex_, currentTime, trs, skeleton);
						keyEdited = true;
					}
				};

			//平行移動
			float t[3] = { trs.translation_.x,trs.translation_.y,trs.translation_.z };
			if (ImGui::DragFloat3("Position", t, 0.1f))
			{
				trs.translation_ = { t[0],t[1],t[2] };
				onPoseChanged();
			}

			//回転(表示中のオイラー角を保持して、±90度を越えても連続して回せるようにする)
			XMFLOAT4 q = trs.rotation_;
			float dot = q.x * eulerCacheQuat_.x + q.y * eulerCacheQuat_.y
				+ q.z * eulerCacheQuat_.z + q.w * eulerCacheQuat_.w;
			if (eulerCacheBone_ != selectedBoneIndex_ || std::abs(dot) < 0.99999f)
			{
				//ボーンが替わった、または時刻移動やUndoで回転が外から変わったので計算し直す
				eulerCache_ = QuaternionToEuler(q);
				eulerCacheQuat_ = q;
				eulerCacheBone_ = selectedBoneIndex_;
			}

			float r[3] = { eulerCache_.x, eulerCache_.y, eulerCache_.z };
			if (ImGui::DragFloat3("Rotation", r, 0.5f))
			{
				eulerCache_ = { WrapDegrees(r[0]), WrapDegrees(r[1]), WrapDegrees(r[2]) };
				trs.rotation_ = EulerToQuaternion(eulerCache_);
				eulerCacheQuat_ = trs.rotation_;
				onPoseChanged();
			}

			//拡縮
			float s[3] = { trs.scale_.x,trs.scale_.y,trs.scale_.z };
			if (ImGui::DragFloat3("Scale", s, 0.01f, 0.01f, 10.0f))
			{
				trs.scale_ = { s[0],s[1],s[2] };
				onPoseChanged();
			}

			//元の姿勢に戻す
			if (ImGui::Button("Reset", ImVec2(-1, 24)))
			{
				//クリップがあれば元の動きの姿勢、無ければバインドポーズに戻す
				trs = currentClip
					? currentClip->SampleBaseBone(selectedBoneIndex_, currentTime, skeleton)
					: bone.bindPoseLocal_;
				onPoseChanged();
			}

			ImGui::Separator();

			//キーフレーム登録、区間置換操作UI
			if (currentClip != nullptr)
			{
				//文字入力中はショートカットを無効にする
				bool canShortcut = !ImGui::GetIO().WantTextInput;

				//単発キー(ボタン or Kキー)
				if (ImGui::Button("Key Selected Bone (K)", ImVec2(-1, 28))
					|| (canShortcut && ImGui::IsKeyPressed(ImGuiKey_K, false)))
				{
					undoStack.Record(clipIndex, *currentClip);
					currentClip->SetOffsetKey(selectedBoneIndex_, currentTime, trs, skeleton);
					keyEdited = true;
				}

				//再生位置のキーを削除(ボタン or Deleteキー)
				if (ImGui::Button("Delete Key (Del)", ImVec2(-1, 24))
					|| (canShortcut && ImGui::IsKeyPressed(ImGuiKey_Delete, false)))
				{
					if (currentClip->HasOffsetKey(selectedBoneIndex_, currentTime))
					{
						undoStack.Record(clipIndex, *currentClip);
						currentClip->RemoveOffsetKey(selectedBoneIndex_, currentTime);
						keyEdited = true;
					}
				}

				//今の時刻の前後に修正0のキーを打ち、修正の影響範囲を限定する
				ImGui::SetNextItemWidth(120.0f);
				ImGui::DragFloat("##LimitRange", &limitRangeSec_, 0.01f, 0.01f, 5.0f, "+-%.2f sec");
				ImGui::SameLine();
				if (ImGui::Button("Limit Edit Range", ImVec2(-1, 0)))
				{
					float dur = currentClip->duration_;
					float before = std::max(0.0f, currentTime - limitRangeSec_);
					float after = (dur > 0.0f) ? std::min(dur, currentTime + limitRangeSec_) : currentTime + limitRangeSec_;

					if (before < currentTime - 0.001f) currentClip->SetZeroOffsetKey(selectedBoneIndex_, before);
					if (after > currentTime + 0.001f)  currentClip->SetZeroOffsetKey(selectedBoneIndex_, after);
					keyEdited = true;
				}
				ImGui::Spacing();
				ImGui::Text("Range Replacement(In-Out):");

				//現在時刻を開始地点としえｔ記録し、この瞬間の姿勢を保存
				if (ImGui::Button("Set In Point", ImVec2(-1, 24)))
				{
					rangeStartTime_ = currentTime;
					rangeStartTrs_ = trs;
					hasRangeStartPose_ = true;
				}

				//In点がセット済みの場合、記録された開始時刻を表示
				if (hasRangeStartPose_)
				{
					ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "In time:%.3f sec(Ready)", rangeStartTime_);
				}
				else
				{
					ImGui::TextDisabled("In Time:Not Set");
				}

				//現在時刻をOut点をして、In点～Out点間の既存キーを一掃して補間置換
				if (ImGui::Button("Set Out&Replace Range", ImVec2(-1, 28)))
				{
					rangeEndTime_ = currentTime;
					Transform startPose = hasRangeStartPose_ ? rangeStartTrs_ : trs;
					undoStack.Record(clipIndex, *currentClip);
					currentClip->ReplaceOffsetRange(selectedBoneIndex_, rangeStartTime_, rangeEndTime_, startPose, trs, skeleton);

					hasRangeStartPose_ = false;
					keyEdited = true;
				}
			}
			else
			{
				//アニメーションクリップが１つも読み込まれていない場合
				ImGui::TextDisabled("Load an Animation clip to edit keys");
			}
		}
	}
	else
	{
		//ボーンが何も選択されていない場合のテキスト
		ImGui::TextDisabled("Select a bone from hierarchy to edit");
	}
	//ドラッグが終わったら、次の編集で新しくUndo履歴を積めるようにする
	if (!ImGui::IsAnyItemActive())
	{
		editRecorded_ = false;
	}
	ImGui::End();

	return keyEdited;
}

void InspectorView::DrawBoneNode(int boneIndex, const Skeleton& skeleton)
{
	//対象ボーンのデータ参照を取得
	const auto& bone = skeleton.GetBone(boneIndex);

	//このボーンの親として持つ子ボーンのインデっクス一覧を収集
	std::vector<int>children;
	for (size_t i = 0; i < skeleton.GetBoneCount(); ++i)
	{
		if (skeleton.GetBone(i).parentIndex_ == boneIndex)
		{
			children.push_back(static_cast<int>(i));
		}
	}

	//ツリーノードの表示オプションフラグを設定
	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;

	//現在選択中のボーンであれば青色ハイライト
	if (selectedBoneIndex_ == boneIndex)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}
	//子ボーンを持たない末端ボーンは矢印をださないリーフノードにする
	if (children.empty())
	{
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}

	//ImGui　ツリーノードを描画
	bool nodeOpen = ImGui::TreeNodeEx(
		reinterpret_cast<void*>(static_cast<intptr_t>(boneIndex)),
		flags, "%s", bone.name_.c_str());

	//このボーン項目がマウスクリックされたら選択状態を更新し、In点記録フラグをリセット
	if (ImGui::IsItemClicked())
	{
		if (selectedBoneIndex_ != boneIndex)
		{
			selectedBoneIndex_ = boneIndex;
			hasRangeStartPose_ = false;
		}
	}

	//子ボーンが存在し、かつツリーノードが開かれている場合は再帰的に子を描画
	if (!children.empty() && nodeOpen)
	{
		for (int childIdx : children)
		{
			DrawBoneNode(childIdx, skeleton);
		}
		ImGui::TreePop();
	}
}
