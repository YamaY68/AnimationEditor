#pragma once
#include<DxLib.h>
#include<vector>
#include<string>

#include "Animation/AnimationClip.h"
#include "Animation/Skeleton.h"
#include "Animation/PoseBlender.h"
#include "Camera/Camera.h"
#include "Fbx/FbxModelLoader.h"
#include "Fbx/FbxMeshModel.h"
#include "Fbx/FbxAnimationExporter.h"
#include "Edit/EditorState.h"

#include "UI/ImGuiManager.h"
#include "UI/MainMenuBar.h"
#include "UI/TimeLineView.h"
#include "UI/InspectorView.h"

#include "Viewport/BonePicker.h"
#include "Viewport/BoneGizmo.h"
#include "Edit/PoseEdit.h"
#include "Animation/TwoBoneIK.h"
#include "Edit/IKPinSet.h"
/// <summary>
/// アプリケーション全体
/// 1フレームの流れ:入力 → UI → ポーズの更新 → 描画
/// </summary>
class Application
{
public:
	Application(void) = default;
	~Application(void) = default;

	//DxLib・ImGuiを初期化し、初期モデルを読み込む
	bool Init(void);
	//メインループ(ウィンドウが閉じられるかESCが押されるまで)
	void Run(void);
	//後片付け
	void Release(void);

private:
	//---------- 1フレームの処理 ----------

	//1フレーム分の処理(下の関数を順番に呼ぶ)
	void UpdateFrame(void);
	//ショートカットキー(Undo/Redo、ボーン表示の切り替え)
	void HandleShortcuts(void);
	//カーソルが重なっているボーンを求める
	void UpdateHoveredBone(void);
	//ビューポートの左クリックで、ボーンを選択する(何もない所なら選択を外す)
	void HandleBonePicking(void);
	//ボーンを選択中に右クリックしたら、ギズモの移動と回転を切り替える
	void HandleGizmoToggleClick(void);
	//UI(メニュー・タイムライン・インスペクター)を描画し、操作を反映する
	//戻り値:キーが変更されたか(ポーズを計算し直す必要があるか)
	bool UpdateUI(float deltaTime);
	//指定したクリップの中身がまだ読み込まれていなければ読み込む(遅延読み込み)
	void EnsureClipLoaded(int clipIndex);
	//アニメーションからポーズを求め、スキニングまで行う
	void UpdatePose(bool forceResample);
	//今のクリップ(とブレンド先)からポーズを求める
	void SampleAnimation(const AnimationClip& clip);
	//3Dの描画(グリッド・モデル・ボーン)
	void Render(void)const;

	//---------- 描画の部品 ----------
	void DrawGridAndAxes(void)const;
	void DrawModel(void)const;
	void DrawBones(void)const;

	//---------- 読み込み ----------
	void LoadDefaultModel(void);
	//ドラッグ＆ドロップされたFBXを読み込む
	void ProcessDroppedFiles(void);
	//モデル(スケルトン＋メッシュ)を読み込む。失敗したら今のモデルはそのまま
	bool LoadParentModel(const std::string& filePath);
	//カメラをモデル全体が見える位置に合わせる
	void FrameCameraToModel(void);
	//今のモデルにアニメーションを追加する
	void AddAnimation(const std::string& filePath);
	//読み込んだクリップに名前を付けて追加し、最初の1本を選択する
	void AddClips(std::vector<AnimationClip>&& clips, const std::string& filePath);

	//---------- 書き出し ----------
	//編集したアニメーションをFBXに書き出す(allClips:全クリップか、今のクリップだけか)
	void ExportFbx(const std::string& filePath, bool allClips);

	//---------- ポーズを計算し直すかの判定用 ----------
	//前回ポーズを求めたときの条件。これが変わったら計算し直す
	struct SampleInputs
	{
		int clipIndex = -1;
		float time = -1.0f;
		bool blendEnabled = false;
		int blendTargetIndex = -1;
		float blendWeight = -1.0f;
		bool operator==(const SampleInputs&)const = default;
	};

	HWND hWnd_ = nullptr;
	int subLightHandle_ = -1;

	ImGuiManager imguiManager_;
	TimeLineView timelineView_;
	InspectorView inspectorView_;
	BoneGizmo boneGizmo_;
	IKPinSet ikPins_;	//ピン留めしたIK
	int pinnedClipIndex_ = -1;	//ピンを置いたときのクリップ(クリップが替わったらピンを外す)
	Camera camera_;

	FbxModelLoader loader_;
	Skeleton skeleton_;
	FbxMeshModel meshModel_;

	//編集の状態(クリップ・再生・ブレンド・選択中のボーン・Undoなど)
	EditorState state_;
	std::string modelPath_;	//読み込んだモデルのFBX(書き出しのときに開き直す)
	bool skeletonLoaded_ = false;
	bool meshLoaded_ = false;
	bool showBones_ = true;

	Pose currentPose_;		//今のクリップのポーズ
	Pose targetPose_;		//ブレンド先のポーズ
	Pose finalPose_;		//表示するポーズ(ブレンド後。インスペクターで編集するのもこれ)
	std::vector<DirectX::XMFLOAT4X4> worldMatrices_;	//各ボーンのワールド行列

	SampleInputs lastSampleInputs_;	//前回ポーズを求めたときの条件
	bool poseDirty_ = true;			//条件に関係なく、次のフレームでポーズを計算し直すか
	int hoveredBoneIndex_ = -1;	//カーソルが重なっているボーン(-1:無し)
};