#include "TimeLineView.h"
#include "FileDialog.h"
#include <imgui.h>
#include <cmath>
#include <vector>
#include <algorithm>

std::string TimeLineView::Draw(
    HWND hWnd,
    const std::vector<AnimationClip>& clips,
    int& currentClipIndex,
    float& inOutTime,
    float deltaTime,
    bool& outEnableBlend,
    int& outTargetClipIndex,
    float& outBlendWeight,
    int selectedBoneIndex) // ★ 1. 引数の末尾に追加
{
    std::string selectedFbxPath = "";

    // 1. メインメニューバー "File (F)"
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File (F)"))
        {
            if (ImGui::MenuItem("Add Animation (.fbx)...", "Ctrl+O"))
            {
                selectedFbxPath = OpenFbxFileDialog(hWnd);
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (clips.empty() || currentClipIndex < 0 || currentClipIndex >= static_cast<int>(clips.size()))
    {
        return selectedFbxPath;
    }

    const auto& clip = clips[currentClipIndex];
    float duration = clip.duration_;

    // 2. Timeline ウィンドウ
    ImGui::SetNextWindowSize(ImVec2(800, 160), ImGuiCond_FirstUseEver);
    ImGui::Begin("Timeline");

    std::string previewName = "[" + std::to_string(currentClipIndex) + "] " + clip.name_;
    if (ImGui::BeginCombo("Active Clip", previewName.c_str()))
    {
        for (int i = 0; i < static_cast<int>(clips.size()); ++i)
        {
            ImGui::PushID(i);
            bool isSelected = (currentClipIndex == i);
            std::string label = "[" + std::to_string(i) + "] " + clips[i].name_;

            if (ImGui::Selectable(label.c_str(), isSelected))
            {
                currentClipIndex = i;
                inOutTime = 0.0f;
            }
            if (isSelected) ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::Text("| Duration: %.2f s", duration);

    // 再生 / 停止
    if (ImGui::Button(isPlaying_ ? "Pause" : "Play"))
    {
        isPlaying_ = !isPlaying_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        isPlaying_ = false;
        inOutTime = 0.0f;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &isLoop_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::SliderFloat("Speed", &playbackSpeed_, -2.0f, 3.0f, "%.2fx");

    // シークバー
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##TimeSeek", &inOutTime, 0.0f, duration, "%.2f s");

    // =============================================================
    // ★ キーフレームマーカーのオーバーレイ描画
    // =============================================================
    ImVec2 barMin = ImGui::GetItemRectMin();
    ImVec2 barMax = ImGui::GetItemRectMax();
    float barWidth = barMax.x - barMin.x;
    float barHeight = barMax.y - barMin.y;

    if (duration > 0.0f && barWidth > 0.0f)
    {
        // 最前面に確実に描画するため ForegroundDrawList を使用
        ImDrawList* drawList = ImGui::GetForegroundDrawList();

        // 表示対象のキーフレーム時刻を収集
// 表示対象のキーフレーム時刻を収集
        std::vector<float> keyTimes;

        for (const auto& track : clip.boneTracks_)
        {
            // ボーンが選択されているときはそのボーンのみ、未選択時は全ボーン
            if (selectedBoneIndex == -1 || track.boneIndex_ == selectedBoneIndex)
            {
                // ★ ユーザーが手動で打ったキー（isUserEdited_ == true）だけを対象にする
                // （FBX読み込み時の等間隔キーは isUserEdited_ が false なので無視される）
                for (const auto& k : track.translationTrack_.keys_)
                {
                    if (k.isUserEdited_) keyTimes.push_back(k.time_);
                }
                for (const auto& k : track.rotationTrack_.keys_)
                {
                    if (k.isUserEdited_) keyTimes.push_back(k.time_);
                }
                for (const auto& k : track.scaleTrack_.keys_)
                {
                    if (k.isUserEdited_) keyTimes.push_back(k.time_);
                }
            }
        }

        // 重複時刻を間引き
        std::sort(keyTimes.begin(), keyTimes.end());
        keyTimes.erase(std::unique(keyTimes.begin(), keyTimes.end(), [](float a, float b) {
            return std::abs(a - b) < 0.005f;
            }), keyTimes.end());

        // スライダーの上下パディングを考慮した中心Y座標
        float centerY = barMin.y + barHeight * 0.5f;

        for (float kt : keyTimes)
        {
            float normTime = std::clamp(kt / duration, 0.0f, 1.0f);
            float posX = barMin.x + normTime * barWidth;

            // 1. スライダー全体を縦断するオレンジ色の縦線
            drawList->AddLine(
                ImVec2(posX, barMin.y),
                ImVec2(posX, barMax.y),
                IM_COL32(255, 120, 0, 255), // 目立つ濃いオレンジ
                3.0f                        // 線の太さ
            );

            // 2. スライダー中央に配置するダイヤ（ひし形）マーカー（枠内に完全に入るサイズ）
            float r = 5.0f; // 半径
            ImVec2 p[4] = {
                ImVec2(posX, centerY - r), // 上
                ImVec2(posX + r, centerY), // 右
                ImVec2(posX, centerY + r), // 下
                ImVec2(posX - r, centerY)  // 左
            };
            drawList->AddConvexPolyFilled(p, 4, IM_COL32(255, 230, 0, 255)); // 鮮やかな黄色
            drawList->AddPolyline(p, 4, IM_COL32(0, 0, 0, 255), ImDrawFlags_Closed, 1.5f); // 黒い輪郭線
        }
    }

    // アニメーションブレンド設定
    if (clips.size() > 1)
    {
        ImGui::Separator();
        ImGui::Checkbox("Enable Blend", &outEnableBlend);
        if (outEnableBlend)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150.0f);
            if (ImGui::BeginCombo("Target Clip", clips[outTargetClipIndex].name_.c_str()))
            {
                for (int i = 0; i < static_cast<int>(clips.size()); ++i)
                {
                    bool isSelected = (outTargetClipIndex == i);
                    if (ImGui::Selectable(clips[i].name_.c_str(), isSelected))
                    {
                        outTargetClipIndex = i;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            ImGui::SetNextItemWidth(180.0f);
            ImGui::SliderFloat("Blend Weight", &outBlendWeight, 0.0f, 1.0f, "%.2f");
        }
    }

    // 時間進行
    if (isPlaying_)
    {
        inOutTime += deltaTime * playbackSpeed_;
        if (isLoop_)
        {
            if (duration > 0.0f)
            {
                if (inOutTime > duration) inOutTime = std::fmod(inOutTime, duration);
                if (inOutTime < 0.0f) inOutTime += duration;
            }
        }
        else
        {
            if (inOutTime > duration) { inOutTime = duration; isPlaying_ = false; }
            if (inOutTime < 0.0f) { inOutTime = 0.0f; isPlaying_ = false; }
        }
    }

    ImGui::End();
    return selectedFbxPath;
}