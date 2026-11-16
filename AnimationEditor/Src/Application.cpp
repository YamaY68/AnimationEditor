#include "Application.h"
#include<filesystem>
#include<algorithm>
#include<cmath>
#include"Animation/Skeleton.h"

bool Application::Init(void)
{
    ChangeWindowMode(TRUE);
    SetGraphMode(1280, 720, 32);
    SetMainWindowText("Custom 3D Animation Editor");
    SetUseDirect3DVersion(DX_DIRECT3D_11);

    if (DxLib_Init() == -1) return false;

    SetDrawScreen(DX_SCREEN_BACK);
    SetUseZBuffer3D(TRUE);
    SetWriteZBuffer3D(TRUE);
    SetCameraNearFar(1.0f, 3000.0f);

    // サブライト（フィルライト）設定
    subLightHandle_ = CreateDirLightHandle(VGet(0.0f, 0.5f, -1.0f));
    SetLightDifColorHandle(subLightHandle_, GetColorF(0.4f, 0.4f, 0.45f, 1.0f));
    SetLightEnableHandle(subLightHandle_, TRUE);

    hWnd_ = reinterpret_cast<HWND>(GetMainWindowHandle());
    if (!imguiManager_.Initialize(hWnd_))
    {
        DxLib_End();
        return false;
    }

    camera_.SetTarget(VGet(0.0f, 90.0f, 0.0f));
    camera_.SetDistance(180.0f);

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
    meshModel_.ReleaseTextures();
    DeleteLightHandle(subLightHandle_);
    imguiManager_.Finalize();
    loader_.Finalize();
    DxLib_End();
}

void Application::UpdateFrame(void)
{
    ClearDrawScreen();

    ProcessDroppedFiles();

    imguiManager_.BeginFrame();

    camera_.Update(!imguiManager_.WantCaptureMouse());
    camera_.ApplyToDxLib();

    DrawGridAndAxes(200.0f, 20);

    // Bキーでボーン表示の切り替え
    int keyB = CheckHitKey(KEY_INPUT_B);
    if (keyB == 1 && prevKeyB_ == 0)
    {
        showBones_ = !showBones_;
    }
    prevKeyB_ = keyB;

    std::string menuSelectedPath;
    MenuActionType menuAction = timelineView_.Draw(
        hWnd_,
        clips_,
        currentClipIndex_,
        playbackTime_,
        1.0f / 60.0f,
        enableBlend_,
        targetClipIndex_,
        blendWeight_,
        inspectorView_.GetSelectedBoneIndex(),
        menuSelectedPath
    );

    HandleMenuAction(menuAction, menuSelectedPath);
    UpdateAndRenderModel();

    imguiManager_.EndFrame();
    ScreenFlip();

}

void Application::ProcessDroppedFiles(void)
{
    std::vector<std::string> droppedFiles = imguiManager_.PopDroppedFiles();
    for (const auto& file : droppedFiles)
    {
        std::string ext = std::filesystem::path(file).extension().string();
        if (ext == ".fbx" || ext == ".FBX")
        {
            LoadParentModel(file);
        }
    }
}

bool Application::LoadParentModel(const std::string& filePath)
{
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

    meshModel_.ReleaseTextures();

    skeleton_ = std::move(newSkeleton);
    meshModel_ = std::move(newMesh);
    skeletonLoaded_ = true;
    meshLoaded_ = true;

    clips_.clear();
    currentClipIndex_ = 0;
    targetClipIndex_ = 0;
    playbackTime_ = 0.0f;
    prevPlaybackTime_ = -1.0f;
    enableBlend_ = false;
    blendWeight_ = 0.0f;

    currentPose_ = skeleton_.CreateBindPose();
    targetPose_ = skeleton_.CreateBindPose();
    finalPose_ = skeleton_.CreateBindPose();
    worldMatrices_.clear();
    worldMatrices_.resize(skeleton_.GetBoneCount());

    skeleton_.ComputeWorldMatrices(finalPose_, worldMatrices_);
    meshModel_.UpdateSkinning(skeleton_, worldMatrices_);

    return true;
}

void Application::HandleMenuAction(MenuActionType action, const std::string& filePath)
{
    if (action == MenuActionType::AddAnimation && skeletonLoaded_ && !filePath.empty())
    {
        AnimationClip newClip;
        if (loader_.LoadAnimation(filePath, skeleton_, newClip))
        {
            newClip.name_ = std::filesystem::path(filePath).stem().string();
            clips_.push_back(std::move(newClip));
            currentClipIndex_ = static_cast<int>(clips_.size()) - 1;
            playbackTime_ = 0.0f;
            prevPlaybackTime_ = -1.0f;
        }
    }
}

void Application::UpdateAndRenderModel(void)
{
    if (!skeletonLoaded_) return;

    AnimationClip* activeClipPtr = nullptr;
    if (!clips_.empty() && currentClipIndex_ >= 0 && currentClipIndex_ < static_cast<int>(clips_.size()))
    {
        activeClipPtr = &clips_[currentClipIndex_];
    }

    bool keyEdited = inspectorView_.Draw(skeleton_, finalPose_, activeClipPtr, playbackTime_);
    bool timeChanged = (std::abs(playbackTime_ - prevPlaybackTime_) > 0.0001f);

    if (activeClipPtr)
    {
        if (timelineView_.IsPlaying() || timeChanged || keyEdited)
        {
            currentPose_ = skeleton_.CreateBindPose();
            activeClipPtr->Sample(playbackTime_, skeleton_, currentPose_);

            if (enableBlend_ && targetClipIndex_ >= 0 && targetClipIndex_ < static_cast<int>(clips_.size()))
            {
                targetPose_ = skeleton_.CreateBindPose();
                clips_[targetClipIndex_].Sample(playbackTime_, skeleton_, targetPose_);
                PoseBlender::Blend(currentPose_, targetPose_, blendWeight_, finalPose_);
            }
            else
            {
                finalPose_ = currentPose_;
            }
        }
    }
    prevPlaybackTime_ = playbackTime_;

    skeleton_.ComputeWorldMatrices(finalPose_, worldMatrices_);
    if (meshLoaded_)
    {
        meshModel_.UpdateSkinning(skeleton_, worldMatrices_);
        SetUseLighting(FALSE);
        SetUseBackCulling(FALSE);
        meshModel_.Draw();
        SetUseLighting(TRUE);
        SetUseBackCulling(TRUE);
    }

    if (showBones_)
    {
        SetUseZBuffer3D(FALSE);
        skeleton_.DrawDebugBones(worldMatrices_, GetColor(255, 255, 0), GetColor(0, 255, 255));

        int selIdx = inspectorView_.GetSelectedBoneIndex();
        if (selIdx >= 0 && selIdx < static_cast<int>(worldMatrices_.size()))
        {
            VECTOR selPos = VGet(worldMatrices_[selIdx]._41, worldMatrices_[selIdx]._42, worldMatrices_[selIdx]._43);
            DrawSphere3D(selPos, 1.5f, 12, GetColor(255, 128, 0), GetColor(255, 128, 0), TRUE);
        }
        SetUseZBuffer3D(TRUE);
    }
}

void Application::DrawGridAndAxes(float size, int divisions) const
{
    float half = size * 0.5f;
    float step = size / divisions;
    unsigned int gridColor = GetColor(70, 70, 70);

    for (int i = 0; i <= divisions; ++i)
    {
        float pos = -half + i * step;
        DrawLine3D(VGet(pos, 0.0f, -half), VGet(pos, 0.0f, half), gridColor);
        DrawLine3D(VGet(-half, 0.0f, pos), VGet(half, 0.0f, pos), gridColor);
    }

    DrawLine3D(VGet(0.0f, 0.0f, 0.0f), VGet(10.0f, 0.0f, 0.0f), GetColor(255, 0, 0));
    DrawLine3D(VGet(0.0f, 0.0f, 0.0f), VGet(0.0f, 10.0f, 0.0f), GetColor(0, 255, 0));
    DrawLine3D(VGet(0.0f, 0.0f, 0.0f), VGet(0.0f, 0.0f, 10.0f), GetColor(0, 0, 255));
}