#include "Application.h"
#include<imgui.h>
#include<filesystem>
#include<algorithm>
#include<cctype>

namespace
{
	//---------- ウィンドウ ----------
	constexpr int kScreenWidth = 1280;
	constexpr int kScreenHeight = 720;
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

	//---------- その他 ----------
	//1フレームの経過時間(秒)。60fpsで動く前提の固定値
	constexpr float kFixedDeltaTime = 1.0f / 60.0f;
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
	const float deltaTime = kFixedDeltaTime;
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

	//Bキー：ボーン表示の切り替え
	if (!io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_B, false))
	{
		showBones_ = !showBones_;
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

	//タイムライン(再生時刻が進む)
	timelineView_.Draw(state_, deltaTime);

	//インスペクター(モデルが無いときは編集するものが無いので出さない)
	if (!skeletonLoaded_) return false;
	const InspectorResult inspector = inspectorView_.Draw(skeleton_, finalPose_, state_);

	//姿勢をいじったら再生を止める(再生中だと次のフレームで上書きされるため)
	if (inspector.poseEdited)
	{
		state_.playback.isPlaying = false;
	}

	return inspector.keyEdited;
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
	SetUseZBuffer3D(FALSE);
	skeleton_.DrawDebugBones(worldMatrices_, GetColor(255, 255, 0), GetColor(0, 255, 255));

	//選択中のボーンにオレンジの球を描く
	const int selected = state_.selectedBoneIndex;
	if (selected >= 0 && selected < static_cast<int>(worldMatrices_.size()))
	{
		const unsigned int color = GetColor(255, 128, 0);
		DrawSphere3D(GetPosition(worldMatrices_[selected]), kSelectedBoneRadius, kSelectedBoneDivisions, color, color, TRUE);
	}
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
	skeletonLoaded_ = true;
	meshLoaded_ = true;

	//モデルが替わったので、クリップ・選択・Undoなどをリセットする
	state_.ResetForNewModel();
	//モデルのFBXにアニメーションが入っていれば、クリップとして追加する
	std::vector<AnimationClip> embeddedClips;
	if (loader_.LoadEmbeddedAnimations(skeleton_, embeddedClips) > 0)
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

void Application::AddAnimation(const std::string& filePath)
{
	if (!skeletonLoaded_ || filePath.empty()) return;

	std::vector<AnimationClip> newClips;
	if (loader_.LoadAnimations(filePath, skeleton_, newClips) > 0)
	{
		AddClips(std::move(newClips), filePath);
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