#include "Application.h"
#include<imgui.h>
#include<filesystem>
#include<algorithm>
#include<cctype>
#include<cmath>
namespace
{
	//---------- ウィンドウ ----------
	constexpr int kScreenWidth = 1600;
	constexpr int kScreenHeight = 900;
	constexpr int kColorBitDepth = 32;

	//---------- カメラ ----------
	constexpr float kCameraNear = 1.0f;
	constexpr float kCameraFar = 3000.0f;
	constexpr VECTOR kDefaultCameraTarget = { 0.0f, 90.0f, 0.0f };
	constexpr float kDefaultCameraDistance = 180.0f;

	//---------- ライト ----------
	constexpr VECTOR kFillLightDirection = { 0.0f, 0.5f, -1.0f };	//補助ライト(影になる側を少し明るくする)の向き

	//---------- グリッド ----------
	constexpr float kGridSize = 200.0f;		//グリッド全体の大きさ
	constexpr int kGridDivisions = 20;		//グリッドの分割数
	constexpr float kAxisLength = 10.0f;	//原点に描くXYZ軸の長さ

	//---------- ボーン表示 ----------
	constexpr float kSelectedBoneRadius = 1.5f;	//選択中のボーンに描く球の半径
	constexpr int kSelectedBoneDivisions = 12;	//その球の分割数
	constexpr float kBonePickRadiusPx = 12.0f;	//クリックで関節を選べる距離(画面上のピクセル)
	constexpr float kBoneLinePickRadiusPx = 6.0f;	//クリックでラインを選べる距離(画面上のピクセル)	//---------- その他 ----------
	constexpr float kJointRadius = 1.0f;	//関節の球の半径
	constexpr float kBoneRadius = 0.3f;		//ボーンの太さ(カプセルの半径)
	constexpr float kPinRadius = 2.0f;	//ピンの位置に描く球の半径
	constexpr float kPoleRadius = 2.2f;	//ポールの目標の位置に描く球の半径
	//1フレームの経過時間の上限(秒)
	//(ブレークポイントで止めたときや、ウィンドウをドラッグしている間に時間が大きく飛ばないように)
	constexpr float kMaxDeltaTime = 0.1f;
	constexpr float kMicroSecondsToSeconds = 1.0e-6f;
	const char* const kDefaultModelPath = "Data/DefaultModel1.fbx";

	//ワールド行列の平行移動成分(＝ボーンの位置)を取り出す
	VECTOR GetPosition(const DirectX::XMFLOAT4X4& m)
	{
		return VGet(m._41, m._42, m._43);
	}
}

//=============================================================
// 初期化・メインループ・後片付け
//=============================================================

bool Application::Init(void)
{
	ChangeWindowMode(TRUE);
	SetGraphMode(kScreenWidth, kScreenHeight, kColorBitDepth);
	SetMainWindowText("AnimationEditor");
	SetUseDirect3DVersion(DX_DIRECT3D_11);

	if (DxLib_Init() == -1) return false;

	SetDragFileValidFlag(TRUE);	//ファイルのドラッグ＆ドロップを受け付ける
	SetDrawScreen(DX_SCREEN_BACK);
	SetUseZBuffer3D(TRUE);
	SetWriteZBuffer3D(TRUE);
	SetCameraNearFar(kCameraNear, kCameraFar);
	SetUseLighting(TRUE);

	//補助ライト
	subLightHandle_ = CreateDirLightHandle(kFillLightDirection);
	SetLightDifColorHandle(subLightHandle_, GetColorF(0.4f, 0.4f, 0.45f, 1.0f));
	SetLightEnableHandle(subLightHandle_, TRUE);

	hWnd_ = reinterpret_cast<HWND>(GetMainWindowHandle());
	if (!imguiManager_.Initialize(hWnd_))
	{
		DxLib_End();
		return false;
	}

	camera_.SetTarget(kDefaultCameraTarget);
	camera_.SetDistance(kDefaultCameraDistance);

	LoadDefaultModel();
	return true;
}

void Application::Run(void)
{
	while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0)
	{
		UpdateFrame();
	}
}

void Application::Release(void)
{
	//テクスチャはDxLibの機能なので、DxLib_Endより前に解放する
	meshModel_.ReleaseTextures();
	DeleteLightHandle(subLightHandle_);
	imguiManager_.Finalize();
	loader_.Finalize();
	DxLib_End();
}

//=============================================================
// 1フレームの処理
//=============================================================

void Application::UpdateFrame(void)
{
	//前のフレームから実際に経過した時間(秒)
	//(決め打ちの1/60秒だと、DebugとReleaseなど処理の速さが違うと再生速度が変わってしまうため)
	const LONGLONG now = GetNowHiPerformanceCount();
	const float elapsed = (lastFrameTime_ > 0) ? static_cast<float>(now - lastFrameTime_) * kMicroSecondsToSeconds : 0.0f;
	const float deltaTime = (std::min)(elapsed, kMaxDeltaTime);	//(std::min)と括弧で囲むのは、Windowsのminマクロと区別するため
	lastFrameTime_ = now;
	ClearDrawScreen();
	ProcessDroppedFiles();

	imguiManager_.BeginFrame();
	HandleShortcuts();

	//ImGuiのウィンドウを操作しているときは、カメラを動かさない
	camera_.Update(!imguiManager_.WantCaptureMouse());
	camera_.ApplyToDxLib();

	//UI → ポーズ → 描画 の順に処理する
	//(UIで再生時刻・クリップの選択・姿勢の編集が変わるので、それを反映してからポーズを求める)
	const bool keyEdited = UpdateUI(deltaTime);
	if (skeletonLoaded_)
	{
		UpdatePose(keyEdited);
		UpdateHoveredBone();
		HandleBonePicking();
		HandleGizmoToggleClick();
	}
	Render();

	imguiManager_.EndFrame();
	ScreenFlip();
}



void Application::HandleShortcuts(void)
{
	const ImGuiIO& io = ImGui::GetIO();
	//文字入力中はショートカットを無効にする
	if (io.WantTextInput) return;

	//Tabキー：操作方法の一覧の表示の切り替え
	if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Tab, false))
	{
		showHelp_ = !showHelp_;
	}


	//Bキー：ボーン表示の切り替え
	if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B, false))
	{
		showBones_ = !showBones_;
	}

	//ギズモの切り替え(ドラッグ中は切り替えない)
//W:移動 E:回転 X:ローカル/ワールド
	if (!io.KeyCtrl && !boneGizmo_.IsUsing())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_W, false)) boneGizmo_.SetOperation(BoneGizmo::Operation::Translate);
		if (ImGui::IsKeyPressed(ImGuiKey_E, false)) boneGizmo_.SetOperation(BoneGizmo::Operation::Rotate);
		if (ImGui::IsKeyPressed(ImGuiKey_I, false)) boneGizmo_.SetOperation(BoneGizmo::Operation::IK);
		if (ImGui::IsKeyPressed(ImGuiKey_X, false)) boneGizmo_.ToggleSpace();
	}
	//P:選択中の先端ボーンをピン留め・解除(IKのチェーンが作れるボーンだけ)
	if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false))
	{
		const int bone = state_.selectedBoneIndex;
		int rootBone = -1;
		int midBone = -1;
		if (TwoBoneIK::FindChain(skeleton_, bone, rootBone, midBone))
		{
			ikPins_.Toggle(bone, worldMatrices_);
		}
	}

	//Ctrl+Z：元に戻す　Ctrl+Y / Ctrl+Shift+Z：やり直す
	if (!io.KeyCtrl) return;
	const bool undo = ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift;
	const bool redo = ImGui::IsKeyPressed(ImGuiKey_Y) || (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyShift);

	int clipIndex = state_.currentClipIndex;
	bool changed = false;
	if (undo)      changed = state_.undoStack.Undo(state_.clips, clipIndex);
	else if (redo) changed = state_.undoStack.Redo(state_.clips, clipIndex);

	if (changed)
	{
		//戻した操作のクリップに切り替え、ポーズを計算し直す
		state_.currentClipIndex = clipIndex;
		poseDirty_ = true;
	}
}

bool Application::UpdateUI(float deltaTime)
{
	//メニュー
	const MenuResult menu = MainMenuBar::Draw(hWnd_);
	if (menu.action == MenuAction::AddAnimation)
	{
		AddAnimation(menu.filePath);
	}

	else if (menu.action == MenuAction::ExportCurrentClip || menu.action == MenuAction::ExportAllClips)
	{
		ExportFbx(menu.filePath, menu.action == MenuAction::ExportAllClips);
	}

	//操作方法の一覧(モデルが無いときも出せるように、ここで描く)
	ControlsHelp::Draw(showHelp_, imguiManager_.GetJapaneseFont());


	//タイムライン(再生時刻が進む。キーを移したり消したりしたらtrue)
	const bool timelineKeyEdited = timelineView_.Draw(state_, deltaTime);


	//クリップが替わったら、ピンを外す(前のクリップの姿勢で置いたピンなので)
	if (state_.currentClipIndex != pinnedClipIndex_)
	{
		ikPins_.Clear();
		pinnedClipIndex_ = state_.currentClipIndex;
	}

	//タイムラインで選ばれたクリップ(とブレンド先)の中身がまだなら、ここで読み込む
//(インスペクターが元の動きを使うので、インスペクターより前に読み込んでおく)
	EnsureClipLoaded(state_.currentClipIndex);
	if (state_.blend.enabled)
	{
		EnsureClipLoaded(state_.blend.targetClipIndex);
	}

	//インスペクター(モデルが無いときは編集するものが無いので出さない)
	if (!skeletonLoaded_) return false;
	const InspectorResult inspector = inspectorView_.Draw(skeleton_, finalPose_, state_);
	bool poseEdited = inspector.poseEdited;
	bool keyEdited = inspector.keyEdited;

	//ギズモ(ボーン表示中だけ、選択中のボーンに出す)
	//IKのときは、根元・中間・先端の3本が書き換わる
	const int gizmoBone = showBones_ ? state_.selectedBoneIndex : -1;
	//ピンにポールが固a定されていれば、ギズモもその位置を使う
	const IKPin* gizmoPin = ikPins_.Find(gizmoBone);
	const DirectX::XMFLOAT3* pinnedPole = (gizmoPin != nullptr && gizmoPin->hasPole) ? &gizmoPin->polePosition : nullptr;
	const std::vector<int> gizmoEdited = boneGizmo_.Draw(skeleton_, worldMatrices_, finalPose_, gizmoBone, pinnedPole);
	for (int bone : gizmoEdited)
	{
		poseEdited = true;
		if (PoseEdit::OnPoseChanged(finalPose_[bone], bone, skeleton_, state_))
		{
			keyEdited = true;
		}
	}

	//ピン留めした先端をIKで動かしたら、ピンもそこへ移す(ポールを動かしたら、ピンにポールを持たせる)
	if (!gizmoEdited.empty()
		&& boneGizmo_.GetOperation() == BoneGizmo::Operation::IK
		&& ikPins_.Has(gizmoBone))
	{
		if (boneGizmo_.GetIKMode() == BoneGizmo::IKMode::Pole)
		{
			ikPins_.SetPole(gizmoBone, boneGizmo_.GetIKPolePosition());
		}
		else
		{
			ikPins_.Set(gizmoBone, boneGizmo_.GetIKTargetPosition(), boneGizmo_.GetIKTargetRotation());
		}
	}

	//姿勢をいじったら、ピン留めした先端がその場に残るように解き直す
	if (poseEdited && !ikPins_.IsEmpty())
	{
		for (int bone : ikPins_.Apply(skeleton_, finalPose_))
		{
			if (PoseEdit::OnPoseChanged(finalPose_[bone], bone, skeleton_, state_))
			{
				keyEdited = true;
			}
		}
	}

	//ドラッグが終わったら、次の編集で新しくUndoの履歴を積めるようにする
	if (!ImGui::IsAnyItemActive() && !boneGizmo_.IsUsing())
	{
		state_.editRecorded = false;
	}

	//姿勢をいじったら再生を止める(再生中だと次のフレームで上書きされるため)
	if (poseEdited)
	{
		state_.playback.isPlaying = false;
	}

	return keyEdited || timelineKeyEdited;
}

void Application::EnsureClipLoaded(int clipIndex)
{
	if (!state_.IsValidClipIndex(clipIndex)) return;

	AnimationClip& clip = state_.clips[clipIndex];
	if (clip.isLoaded_) return;

	if (!loader_.LoadEmbeddedAnimation(skeleton_, clip))
	{
		OutputDebugStringA(("[Load] アニメーションの読み込み失敗: " + clip.name_ + "\n").c_str());
	}
	//失敗しても読み込み済みにする(毎フレーム読み込みを試して重くならないように)
	clip.isLoaded_ = true;
	poseDirty_ = true;
}

void Application::UpdatePose(bool forceResample)
{
	if (const AnimationClip* clip = state_.GetActiveClip())
	{
		//ポーズを決める条件
		const SampleInputs inputs = {
			state_.currentClipIndex,
			state_.playback.time,
			state_.blend.enabled,
			state_.blend.targetClipIndex,
			state_.blend.weight };

		//条件が変わったときだけ計算し直す
		//(何も変わっていないのに毎フレーム計算し直すと、オートキーOFFでいじった姿勢が元に戻ってしまう)
		const bool needResample = forceResample || poseDirty_ || state_.playback.isPlaying
			|| !(inputs == lastSampleInputs_);
		if (needResample)
		{
			SampleAnimation(*clip);
			lastSampleInputs_ = inputs;
			poseDirty_ = false;
		}
	}

	//ポーズ → 各ボーンのワールド行列 → 頂点の変形
	skeleton_.ComputeWorldMatrices(finalPose_, worldMatrices_);
	if (meshLoaded_)
	{
		meshModel_.UpdateSkinning(skeleton_, worldMatrices_);
	}
}

void Application::SampleAnimation(const AnimationClip& clip)
{
	const float time = state_.playback.time;

	//今のクリップのポーズ(キーの無いボーンはバインドポーズのまま)
	currentPose_ = skeleton_.CreateBindPose();
	clip.Sample(time, skeleton_, currentPose_);

	//ブレンドが有効なら、ブレンド先のポーズと混ぜる
	const BlendState& blend = state_.blend;
	if (blend.enabled && state_.IsValidClipIndex(blend.targetClipIndex))
	{
		targetPose_ = skeleton_.CreateBindPose();
		state_.clips[blend.targetClipIndex].Sample(time, skeleton_, targetPose_);
		PoseBlender::Blend(currentPose_, targetPose_, blend.weight, finalPose_);
	}
	else
	{
		finalPose_ = currentPose_;
	}
}

//=============================================================
// 描画
//=============================================================

void Application::Render(void) const
{
	DrawGridAndAxes();

	if (!skeletonLoaded_) return;
	if (meshLoaded_) DrawModel();
	if (showBones_) DrawBones();
}

void Application::DrawGridAndAxes(void) const
{
	const float half = kGridSize * 0.5f;
	const float step = kGridSize / kGridDivisions;
	const unsigned int gridColor = GetColor(70, 70, 70);

	for (int i = 0; i <= kGridDivisions; ++i)
	{
		const float pos = -half + i * step;
		DrawLine3D(VGet(pos, 0.0f, -half), VGet(pos, 0.0f, half), gridColor);
		DrawLine3D(VGet(-half, 0.0f, pos), VGet(half, 0.0f, pos), gridColor);
	}

	//原点のXYZ軸(X:赤 Y:緑 Z:青)
	const VECTOR origin = VGet(0.0f, 0.0f, 0.0f);
	DrawLine3D(origin, VGet(kAxisLength, 0.0f, 0.0f), GetColor(255, 0, 0));
	DrawLine3D(origin, VGet(0.0f, kAxisLength, 0.0f), GetColor(0, 255, 0));
	DrawLine3D(origin, VGet(0.0f, 0.0f, kAxisLength), GetColor(0, 0, 255));
}

void Application::DrawModel(void) const
{
	//ライティングなし・裏面も描く(法線が怪しいモデルでも見えるように)
	SetUseLighting(FALSE);
	SetUseBackCulling(FALSE);
	meshModel_.Draw();
	SetUseLighting(TRUE);
	SetUseBackCulling(TRUE);
}

void Application::DrawBones(void) const
{
	//モデルに隠れないよう、Zバッファを使わずに手前に描く
	//ライトの影響で色が暗くならないよう、ライティングも切る
	SetUseZBuffer3D(FALSE);
	SetUseLighting(FALSE);

	BoneDrawStyle style;
	style.boneColor = GetColor(255, 255, 0);		//黄
	style.jointColor = GetColor(255, 100, 100);		//
	style.selectedColor = GetColor(255, 0, 0);		//選択中(赤)
	style.hoveredColor = GetColor(255, 128, 0);		//カーソルが重なっている(オレンジ)
	style.jointRadius = kJointRadius;
	style.boneRadius = kBoneRadius;

	const int selected = state_.selectedBoneIndex;
	skeleton_.DrawDebugBones(worldMatrices_, style, selected, hoveredBoneIndex_);

	//選択中のボーンに大きい球を描く
	if (selected >= 0 && selected < static_cast<int>(worldMatrices_.size()))
	{
		DrawSphere3D(GetPosition(worldMatrices_[selected]), kSelectedBoneRadius, kSelectedBoneDivisions,
			style.selectedColor, style.selectedColor, TRUE);
	}
	//ピンの位置に球を描く
	const unsigned int pinColor = GetColor(200, 80, 255);
	for (const IKPin& pin : ikPins_.GetPins())
	{
		const VECTOR pos = VGet(pin.position.x, pin.position.y, pin.position.z);
		DrawSphere3D(pos, kPinRadius, kSelectedBoneDivisions, pinColor, pinColor, TRUE);
	}

	//IKモード中は、ひざ・ひじからポールの目標へ線を引き、目標に緑の球を描く
	int rootBone = -1;
	int midBone = -1;
	if (boneGizmo_.IsIKActive() && TwoBoneIK::FindChain(skeleton_, selected, rootBone, midBone))
	{
		const unsigned int poleColor = GetColor(80, 255, 120);
		const DirectX::XMFLOAT3& pole = boneGizmo_.GetIKPolePosition();
		const VECTOR polePos = VGet(pole.x, pole.y, pole.z);
		DrawLine3D(GetPosition(worldMatrices_[midBone]), polePos, poleColor);
		DrawSphere3D(polePos, kPoleRadius, kSelectedBoneDivisions, poleColor, poleColor, TRUE);
	}

	SetUseLighting(TRUE);

	SetUseZBuffer3D(TRUE);
}

//=============================================================
// 読み込み
//=============================================================

void Application::LoadDefaultModel(void)
{
	LoadParentModel(kDefaultModelPath);
}

void Application::ProcessDroppedFiles(void)
{
	//DxLibが受け取ったファイルを1つずつ取り出す(取り出したものはDxLib側のリストから消える)
	char path[MAX_PATH];
	while (GetDragFilePath(path, sizeof(path)) == 0)
	{
		//拡張子は小文字にそろえて比較する(.FBX や .Fbx も受け付ける)
		std::string ext = std::filesystem::path(path).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		if (ext == ".fbx" && !LoadParentModel(path))
		{
			OutputDebugStringA(("[Load] 読み込み失敗: " + std::string(path) + "\n").c_str());
		}
	}
}

bool Application::LoadParentModel(const std::string& filePath)
{
	//一時変数に読み込み、全部成功したときだけ入れ替える(失敗しても今のモデルが残るように)
	Skeleton newSkeleton;
	FbxMeshModel newMesh;

	if (!loader_.LoadSkeleton(filePath, newSkeleton))
	{
		return false;
	}
	newSkeleton.ComputeInverseBindMatrices();

	if (!loader_.LoadMeshWithSkinning(filePath, newSkeleton, newMesh))
	{
		return false;
	}

	skeleton_ = std::move(newSkeleton);
	meshModel_ = std::move(newMesh);	//古いテクスチャはムーブ代入の中で解放される
	modelPath_ = filePath;
	skeletonLoaded_ = true;
	meshLoaded_ = true;

	//モデルが替わったので、クリップ・選択・Undoなどをリセットする
	state_.ResetForNewModel();
	ikPins_.Clear();	//ピンのボーン番号は前のモデルのものなので外す
	//モデルの大きさはモデルごとに違うので、カメラを合わせ直す
	FrameCameraToModel();
	//モデルのFBXにアニメーションが入っていれば、クリップとして追加する
	//(ここでは名前と長さだけ。中身は、そのクリップが選ばれたときに読み込む)
	std::vector<AnimationClip> embeddedClips;
	if (loader_.ListEmbeddedAnimations(embeddedClips) > 0)
	{
		AddClips(std::move(embeddedClips), filePath);
	}
	//バインドポーズを表示する
	currentPose_ = skeleton_.CreateBindPose();
	targetPose_ = skeleton_.CreateBindPose();
	finalPose_ = skeleton_.CreateBindPose();
	poseDirty_ = true;
	return true;
}
void Application::FrameCameraToModel(void)
{
	DirectX::XMFLOAT3 minPos, maxPos;
	if (!meshModel_.GetBindBounds(minPos, maxPos)) return;

	//モデルを囲む箱の中心と、箱の対角線の半分(＝箱がすっぽり入る球の半径)
	const VECTOR center = VGet(
		(minPos.x + maxPos.x) * 0.5f,
		(minPos.y + maxPos.y) * 0.5f,
		(minPos.z + maxPos.z) * 0.5f);
	const float dx = maxPos.x - minPos.x;
	const float dy = maxPos.y - minPos.y;
	const float dz = maxPos.z - minPos.z;
	const float radius = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;

	camera_.Frame(center, radius);
}
void Application::AddAnimation(const std::string& filePath)
{
	if (!skeletonLoaded_ || filePath.empty()) return;

	std::vector<AnimationClip> newClips;
	if (loader_.LoadAnimations(filePath, skeleton_, newClips) > 0)
	{
		AddClips(std::move(newClips), filePath);
	}
}

void Application::ExportFbx(const std::string& filePath, bool allClips)
{
	if (!skeletonLoaded_ || filePath.empty()) return;

	//書き出すクリップを集める(中身をまだ読み込んでいないクリップは、ここで読み込む)
	std::vector<const AnimationClip*> clips;
	for (int i = 0; i < static_cast<int>(state_.clips.size()); ++i)
	{
		if (!allClips && i != state_.currentClipIndex) continue;
		EnsureClipLoaded(i);
		clips.push_back(&state_.clips[i]);
	}

	std::string error;
	if (FbxAnimationExporter::Export(modelPath_, filePath, skeleton_, clips, error))
	{
		const std::string message = "書き出しました(" + std::to_string(clips.size()) + "クリップ)\n" + filePath;
		MessageBoxA(hWnd_, message.c_str(), "Export FBX", MB_OK | MB_ICONINFORMATION);
	}
	else
	{
		MessageBoxA(hWnd_, error.c_str(), "Export FBX", MB_OK | MB_ICONERROR);
	}
}


void Application::AddClips(std::vector<AnimationClip>&& clips, const std::string& filePath)
{
	if (clips.empty()) return;

	//1本だけならファイル名、複数あれば「ファイル名/AnimStack名」にする
	//(AnimStack名は"Take 001"や"mixamo.com"のように、どのファイルでも同じことが多いため)
	const std::string fileName = std::filesystem::path(filePath).stem().string();
	const bool hasMultiple = clips.size() > 1;

	//追加した最初のクリップを選択できるよう、追加前の数を覚えておく
	const int firstNewIndex = static_cast<int>(state_.clips.size());
	for (AnimationClip& clip : clips)
	{
		clip.name_ = hasMultiple ? (fileName + "/" + clip.name_) : fileName;
		state_.clips.push_back(std::move(clip));
	}

	//追加した最初のクリップを、先頭から再生する
	state_.currentClipIndex = firstNewIndex;
	state_.playback.time = 0.0f;
	poseDirty_ = true;
}


void Application::UpdateHoveredBone(void)
{
	hoveredBoneIndex_ = -1;

	//ボーンを表示していない・UIの上・ギズモの上では、ボーンに重なっていない扱いにする
	if (!showBones_) return;
	if (imguiManager_.WantCaptureMouse()) return;
	if (boneGizmo_.IsHovered() || boneGizmo_.IsUsing()) return;

	int mouseX, mouseY;
	GetMousePoint(&mouseX, &mouseY);

	hoveredBoneIndex_ = BonePicker::Pick(skeleton_, worldMatrices_,
		static_cast<float>(mouseX), static_cast<float>(mouseY),
		kBonePickRadiusPx, kBoneLinePickRadiusPx);
}

void Application::HandleBonePicking(void)
{
	//ボーンを表示していない・UIを操作している・ギズモの上・クリックしていないときは何もしない
	if (!showBones_) return;
	if (imguiManager_.WantCaptureMouse()) return;
	if (boneGizmo_.IsHovered() || boneGizmo_.IsUsing()) return;
	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) return;

	//カーソルが重なっているボーンを選ぶ(無ければ-1になり、選択が外れる)
	state_.selectedBoneIndex = hoveredBoneIndex_;
}

void Application::HandleGizmoToggleClick(void)
{
	//ボーンを選んでいない・UIを操作している・ギズモをドラッグ中なら何もしない
	if (!showBones_ || state_.selectedBoneIndex < 0) return;
	if (imguiManager_.WantCaptureMouse()) return;
	if (boneGizmo_.IsUsing()) return;

	//右ボタンを離した瞬間だけ判定する
	if (!ImGui::IsMouseReleased(ImGuiMouseButton_Right)) return;
	//押してから離すまでにマウスが動いていたら、カメラの回転なので切り替えない
	//(MouseDragMaxDistanceSqr:押している間に動いた最大距離の2乗)
	const ImGuiIO& io = ImGui::GetIO();
	const float threshold = io.MouseDragThreshold;
	if (io.MouseDragMaxDistanceSqr[ImGuiMouseButton_Right] >= threshold * threshold) return;
	boneGizmo_.ToggleOperation();
}