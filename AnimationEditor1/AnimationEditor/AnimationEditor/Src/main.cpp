#include <DxLib.h>
#include <cmath>
#include <vector>
#include <algorithm>
#include <filesystem>
#include "Animation/AnimationClip.h"
#include "Animation/Skeleton.h"
#include "Camera/Camera.h"
#include "Animation/PoseBlender.h"
#include "Fbx/FbxImporter.h"
#include "Fbx/FbxMeshModel.h"

// UI
#include "UI/ImGui/ImGuiManager.h"
#include "UI/TimeLineView.h"
#include "UI/InspectorView.h"



void DrawGridAndAxes(float size, int divisions)
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    ChangeWindowMode(TRUE);
    SetGraphMode(1280, 720, 32);

    // -------------------------------------------------------------
    // サブライト（フィルライト）の作成
    // -------------------------------------------------------------
    // 第1引数: 光の向き (前方斜め下から照らす向き: Zマイナス・Y少し上向き)
    int subLightHandle = CreateDirLightHandle(VGet(0.0f, 0.5f, -1.0f));

    // サブライトの色と強さ（メインより少し暗めの明るさに設定して陰影を残す）
    SetLightDifColorHandle(subLightHandle, GetColorF(0.4f, 0.4f, 0.45f, 1.0f));

    // サブライトを有効化
    SetLightEnableHandle(subLightHandle, TRUE);

    if (DxLib_Init() == -1) return -1;
    SetDrawScreen(DX_SCREEN_BACK);
    SetUseZBuffer3D(TRUE);
    SetWriteZBuffer3D(TRUE);
    SetCameraNearFar(1.0f, 3000.0f);

    HWND hWnd = reinterpret_cast<HWND>(GetMainWindowHandle());
    ImGuiManager imguiManager;
    imguiManager.Initialize(hWnd);

    TimeLineView timelineView;
    InspectorView inspectorView;

    Camera camera;
    camera.SetTarget(VGet(0.0f, 10.0f, 0.0f));
    camera.SetDistance(40.0f);

    Editor::FbxImporter importer;
    Skeleton skeleton;
    FbxMeshModel meshModel;
    std::vector<AnimationClip> clips; // 読み込まれたアニメーション群

    bool skeletonLoaded = false;
    bool meshLoaded = false;

    int currentClipIndex = 0;
    int targetClipIndex = 0;
    bool enableBlend = false;
    float blendWeight = 0.0f;

    Pose currentPose, targetPose, finalPose;
    std::vector<DirectX::XMFLOAT4X4> worldMatrices;
    if (skeletonLoaded)
    {
        currentPose = skeleton.CreateBindPose();
        targetPose = skeleton.CreateBindPose();
        finalPose = skeleton.CreateBindPose();
        worldMatrices.resize(skeleton.GetBoneCount());
    }

    float playbackTime = 0.0f;
    bool showBones = true;
    int prevKeyB = 0;

    // 参照キャプチャ [&] で外側の変数を直接操作
    auto ResetAndLoadParentModel = [&](const std::string& path) -> bool {
        Skeleton newSkeleton;
        FbxMeshModel newMesh;

        if (!importer.LoadSkeleton(path, newSkeleton))
        {
            return false;
        }

        if (!importer.LoadMeshWithSkinning(path, newSkeleton, newMesh))
        {
            return false;
        }

        // 前回のテクスチャを解放
        meshModel.ReleaseTextures();

        // 外側の変数へ確実にムーブ代入
        skeleton = std::move(newSkeleton);
        meshModel = std::move(newMesh);
        skeletonLoaded = true;
        meshLoaded = true;

        // アニメーション関連クリア
        clips.clear();
        currentClipIndex = 0;
        targetClipIndex = 0;
        playbackTime = 0.0f;
        enableBlend = false;
        blendWeight = 0.0f;

        // バッファ再確保
        currentPose = skeleton.CreateBindPose();
        targetPose = skeleton.CreateBindPose();
        finalPose = skeleton.CreateBindPose();
        worldMatrices.clear();
        worldMatrices.resize(skeleton.GetBoneCount());

        // ★ 初期バインドポーズを skinnedVertices_ にコピー
        meshModel.ResetToBindPose();

        return true;
        };
    // ★ ラムダ式の閉じカッコ直後に追加
    ResetAndLoadParentModel("Data/TPose.fbx");
    while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0)
    {
        ClearDrawScreen();

        // 1. ファイルドラッグ＆ドロップ（新しい親モデルの読み込み）
        auto droppedFiles = imguiManager.PopDroppedFiles();
        for (const auto& file : droppedFiles)
        {
            if (std::filesystem::path(file).extension() == ".fbx")
            {
                // ドロップされたファイルを新親としてロード（古い親とアニメーションは自動破棄）
                ResetAndLoadParentModel(file);
            }
        }

        imguiManager.BeginFrame();

        camera.Update(!imguiManager.WantCaptureMouse());
        camera.ApplyToDxLib();

        DrawGridAndAxes(200.0f, 20);

        int keyB = CheckHitKey(KEY_INPUT_B);
        if (keyB == 1 && prevKeyB == 0)
        {
            showBones = !showBones;
        }
        prevKeyB = keyB;
        // 2. UI 描画 & メニューバーからの追加アニメーション読み込み
        int selectedBone = inspectorView.GetSelectedBoneIndex(); // ★ 選択中ボーンを取得

        std::string animFileToAdd = timelineView.Draw(
            hWnd, clips, currentClipIndex, playbackTime, 1.0f / 60.0f,
            enableBlend, targetClipIndex, blendWeight,
            selectedBone // ★ 第9引数として渡す
        );

        if (!animFileToAdd.empty() && skeletonLoaded)
        {
            AnimationClip newClip;
            if (importer.LoadAnimation(animFileToAdd, skeleton, newClip))
            {
                clips.push_back(newClip);
                currentClipIndex = static_cast<int>(clips.size()) - 1;
                playbackTime = 0.0f;
            }
        }

        if (skeletonLoaded)
        {
            AnimationClip* activeClipPtr = (!clips.empty() && currentClipIndex < static_cast<int>(clips.size()))
                ? &clips[currentClipIndex] : nullptr;

            // 再生中のみ AnimationClip から姿勢をサンプリング（一時停止中は手動編集した姿勢を保持）
            if (activeClipPtr && timelineView.IsPlaying())
            {
                activeClipPtr->Sample(playbackTime, skeleton, currentPose);

                if (enableBlend && targetClipIndex < static_cast<int>(clips.size()))
                {
                    clips[targetClipIndex].Sample(playbackTime, skeleton, targetPose);
                    PoseBlender::Blend(currentPose, targetPose, blendWeight, finalPose);
                }
                else
                {
                    finalPose = currentPose;
                }
            }

            // ★ インスペクター描画 & ボーンパラメータ直接編集
            inspectorView.Draw(skeleton, finalPose, activeClipPtr, playbackTime);

            // ショートカットキー「K」でもキー登録可能にする
            if (CheckHitKey(KEY_INPUT_K) == 1 && activeClipPtr)
            {
                int selIdx = inspectorView.GetSelectedBoneIndex();
                if (selIdx >= 0 && selIdx < static_cast<int>(finalPose.localTransforms_.size()))
                {
                    BoneTrack* track = activeClipPtr->GetOrCreateTrack(selIdx);
                    track->SetKeyframe(playbackTime, finalPose.localTransforms_[selIdx]);
                }
            }

            // FK計算（常に最新の finalPose から計算）
            skeleton.ComputeWorldMatrices(finalPose, worldMatrices);

            // メッシュのスキニング更新
            if (meshLoaded)
            {
                meshModel.UpdateSkinning(skeleton, worldMatrices);
            }
            // 2. メッシュ描画（常に実行）
            if (meshLoaded)
            {
                SetUseZBuffer3D(TRUE);
                SetWriteZBuffer3D(TRUE);

                SetUseLighting(TRUE);
                SetMaterialUseVertDifColor(TRUE);

                // ① 標準ライト（メイン光）: 真上から真下へ向けて照らす
                ChangeLightTypeDir(VGet(0.0f, -1.0f, 0.0f));
                SetLightDifColor(GetColorF(0.8f, 0.8f, 0.8f, 1.0f));

                // ② サブライトを有効化
                SetLightEnableHandle(subLightHandle, TRUE);

                meshModel.Draw();

                // 描画後は必要に応じてライティングを無効化
                SetUseLighting(FALSE);
                SetLightEnableHandle(subLightHandle, FALSE);
            }

            // 3. ボーン線画描画
            if (showBones)
            {
                SetUseZBuffer3D(FALSE);


                skeleton.DrawDebugBones(worldMatrices, GetColor(0, 220, 255), GetColor(255, 50, 150));

                int selIdx = inspectorView.GetSelectedBoneIndex();
                if (selIdx >= 0 && selIdx < static_cast<int>(worldMatrices.size()))
                {
                    VECTOR selPos = VGet(worldMatrices[selIdx]._41, worldMatrices[selIdx]._42, worldMatrices[selIdx]._43);
                    // 選択されたボーンの球（ハイライトは目立つ赤またはオレンジ）
                    DrawSphere3D(selPos, 1.0f, 12, GetColor(255, 50, 50), GetColor(255, 50, 50), TRUE);
                }
                SetUseZBuffer3D(TRUE);
            }
        }
        imguiManager.EndFrame();
        ScreenFlip();
    }

    imguiManager.Finalize();
    importer.Release();
    DeleteLightHandle(subLightHandle); 
    DxLib_End();
    return 0;
}