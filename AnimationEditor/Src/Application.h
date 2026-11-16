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

#include "UI/ImGuiManager.h"
#include "UI/TimeLineView.h"
#include "UI/InspectorView.h"

class Application
{
public:
	Application(void) = default;
	~Application(void) = default;

	bool Init(void);

	void Run(void);

	void Release(void);

private:
	void UpdateFrame(void);

	void ProcessDroppedFiles(void);
	bool LoadParentModel(const std::string& filePath);
	void HandleMenuAction(MenuActionType action, const std::string& filePath);

	void UpdateAndRenderModel(void);
	void DrawGridAndAxes(float size, int divisions)const;

    HWND hWnd_ = nullptr;
    int subLightHandle_ = -1;

    ImGuiManager imguiManager_;
    TimeLineView timelineView_;
    InspectorView inspectorView_;
    Camera camera_;

    FbxModelLoader loader_;
    Skeleton skeleton_;
    FbxMeshModel meshModel_;
    std::vector<AnimationClip> clips_;

    bool skeletonLoaded_ = false;
    bool meshLoaded_ = false;

    int currentClipIndex_ = 0;
    int targetClipIndex_ = 0;
    bool enableBlend_ = false;
    float blendWeight_ = 0.0f;

    Pose currentPose_;
    Pose targetPose_;
    Pose finalPose_;
    std::vector<DirectX::XMFLOAT4X4> worldMatrices_;

    float playbackTime_ = 0.0f;
    float prevPlaybackTime_ = -1.0f;
    bool showBones_ = true;
    int prevKeyB_ = 0;
};

