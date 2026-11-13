#include "TimeLineView.h"
#include<imgui.h>
#include<cmath>
#include<algorithm>
#include"FileDialogHelper.h"


MenuActionType TimeLineView::Draw(HWND hWnd, const std::vector<AnimationClip>& clips, int& currentClipIndex, float& inOutTime, float deltaTime, bool& outEnableBlend, int& outTargetClipIndex, float& outBlendWeight, int selectedBoneIndex, std::string& outSelectedFilePath)
{
    MenuActionType action = MenuActionType::None;
    //出力先のファイルパス文字列を空にしてリセット
    outSelectedFilePath.clear();

    //画面最上部のメニューバー
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            //親モデルにたいして追加のアニメーションFBXを読み込む
            if (ImGui::MenuItem("AddAnimation(.fbx)"))
            {
                //Windows標準のファイルを開くダイアログを表示してパスを取得
                std::string path = OpenFbxFileDialog(hWnd);
                if (!path.empty())
                {
                    outSelectedFilePath = path;
                    action = MenuActionType::AddAnimation;
                }
            }
            //メニュー内に水平区切り線を描画
            ImGui::Separator();

            //編集済みアニメーションをFBXとして書き出す
            if (ImGui::MenuItem("ExportFBX(.fbx)", nullptr, false, !clips.empty()))
            {
                //Windows標準の名前をつけて保存ダイアログを表示
                std::string path = SaveFbxFileDialog(hWnd);
                if (!path.empty())
                {
                    outSelectedFilePath = path;
                    action = MenuActionType::ExportFbx;
                }
            }
            ImGui::EndMenu();
        }
        //メインメニューバー全体の終了
        ImGui::EndMainMenuBar();
    }
    
    //クリップが一つも読み込まれていない場合はガイド表示
    if (clips.empty())
    {
        ImGui::SetNextWindowSize(ImVec2(500, 80), ImGuiCond_FirstUseEver);
        ImGui::Begin("Timeline");
        ImGui::Text("ベースとなるモデル(.fbx)をドラッグ＆ドロップ");
        ImGui::Text("アニメーションを追加(File->AddAnimation)");
        ImGui::End();
        return action;
    }

    //クリップインデックスの安全範囲クランプ
    if (currentClipIndex < 0 || currentClipIndex >= static_cast<int>(clips.size()))
    {
        currentClipIndex = 0;
    }
    //現在選択されているクリップへの定数参照を取得
    const AnimationClip& activeClip = clips[currentClipIndex];
    //クリップの総再生時間を取得
    float duration = activeClip.duration_;

    //タイムラインウィンドウ本体
    ImGui::SetNextWindowSize(ImVec2(860, 140), ImGuiCond_FirstUseEver);
    ImGui::Begin("Timeline");

    //クリップ選択ドロップダウン
    ImGui::SetNextItemWidth(220.0f);
    //選択中のクリップ名をプレビュー表示するドロップダウン開始
    if (ImGui::BeginCombo("Active Clip", activeClip.name_.c_str()))
    {
        //読み込まれている全クリップをリスト項目として並べる
        for (int i = 0; i < static_cast<int>(clips.size()); ++i)
        {
            //現在選択中の番号と一致しているか判定
            bool isSelected = (currentClipIndex == i);
            //同名クリップがあってもImGuiのIDが衝突しないように
            std::string label = clips[i].name_ + "##" + std::to_string(i);
            //リスト項目がクリックされたらクリップを切替、再生時刻を０に戻す
            if (ImGui::Selectable(label.c_str(), isSelected))
            {
                currentClipIndex = i;
                inOutTime = 0.0f;
            }
            //選択中の項目に初期フォーカスを合わせる
            if (isSelected)
            {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }
    //同じ行に続けてクリップの総再生時間を表示
    ImGui::SameLine();
    ImGui::Text("| Duration:%.2f sec", duration);

    //再生、一時停止、停止、ループ、再生速度
    if (ImGui::Button(isPlaying_ ? "Pause" : "Play", ImVec2(60, 0)))
    {
        isPlaying_ = !isPlaying_;
    }
    //同じ行にStopボタンを配置
    ImGui::SameLine();
    if (ImGui::Button("Stop", ImVec2(50, 0)))
    {
        isPlaying_ = false;
        inOutTime = 0.0f;
    }
    //同じ行にループ再生の有効無効チェックボタンを配置
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &isLoop_);
    //同じ行に再生速度スライダーを配置
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110.0f);
    ImGui::SliderFloat("Speed", &playbackSpeed_, -2.0f, 3.0f, "%.2fx");
    //同じ行に速度を標準へ戻すボタンを配置
    ImGui::SameLine();
    if (ImGui::Button("1.0x"))
    {
        playbackSpeed_ = 1.0f;
    }

    //複数クリップ読み込みのモーションブレンド設定UI
    if (clips.size() >= 2)
    {
        //ブレンド有効かのチェックボックス
        ImGui::Checkbox("Blend", &outEnableBlend);
        if (outEnableBlend)
        {
            ImGui::SameLine();
            //ブレンド相手インデックスが不正な場合はブレンドを強制オフ
            if (outTargetClipIndex < 0 || outTargetClipIndex >= static_cast<int>(clips.size()))
            {
                outEnableBlend = false;
            }
            //ブレンド相手クリップを選ぶドロップダウンリスト
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::BeginCombo("##BlendTarget", clips[outTargetClipIndex].name_.c_str()))
            {
                for (int i = 0; i < static_cast<int>(clips.size()); ++i)
                {
                    bool isSel = (outTargetClipIndex == i);
                    std::string itemLabel = clips[i].name_ + "##blend" + std::to_string(i);
                    if (ImGui::Selectable(itemLabel.c_str(), isSel))
                    {
                        outTargetClipIndex = i;
                    }
                }
                ImGui::EndCombo();
            }
            //ブレンド比率スライダー
            ImGui::SameLine();
            ImGui::SetNextItemWidth(150.0f);
            ImGui::SliderFloat("Weight", &outBlendWeight, 0.0f, 1.0f, "%.2f");
        }
    }
    else
    {
        //クリップが１本しかない場合はブレンドを強制オフにする
        outEnableBlend = false;
    }

    //シークバーとキーフレームマーカーのオーバーレイ描画
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::SliderFloat("##TimeSeek", &inOutTime, 0.0f, (duration > 0.0f ? duration : 1.0f), "%.3f s");

    //シークバーの画面座標領域を取得
    ImVec2 barMin = ImGui::GetItemRectMin();
    ImVec2 barMax = ImGui::GetItemRectMax();
    float barWidth = barMax.x - barMin.x;
    float barHeight = barMax.y - barMin.y;
    //再生時間とバーの幅が有効な場合のみマーカーを描画
    if (duration > 0.0f && barWidth > 0.0f)
    {
        //シークバー背景やつまみの手前に確実に描画するためにForegroundDrawListを仕様
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        //マーカーを描画する時刻を収集する配列
        std::vector<float>keyTimes;

        // クリップ内の全ボーントラックを走査
        for (const auto& track : activeClip.boneTracks_)
        {
            // ボーン未選択(-1)なら全ボーン、選択中ならそのボーンのトラックのみ対象にする
            if (selectedBoneIndex == -1 || track.boneIndex_ == selectedBoneIndex)
            {
                // 手動編集フラグ（isUserEdited_ == true）が立っているキーの時刻のみを収集
                for (const auto& k : track.translationTrack_.keys_) { if (k.isUserEdited_) keyTimes.push_back(k.time_); }
                for (const auto& k : track.rotationTrack_.keys_) { if (k.isUserEdited_) keyTimes.push_back(k.time_); }
                for (const auto& k : track.scaleTrack_.keys_) { if (k.isUserEdited_) keyTimes.push_back(k.time_); }
            }
        }

        //時刻準にソートし、ほぼ同時刻の重複マーカーを間引き
        std::sort(keyTimes.begin(), keyTimes.end());
        keyTimes.erase(std::unique(keyTimes.begin(), keyTimes.end(), [](float a, float b)
            {
                return std::abs(a - b) < 0.005f; 
            }), keyTimes.end());

        float centerY = barMin.y + barHeight * 0.5f;

        //各キーフレーム時刻に黄色いひし形マーカーを描画
        for (float kt : keyTimes)
        {
            float normTime = std::clamp(kt / duration, 0.0f, 1.0f);
            float posX = barMin.x + normTime * barWidth;

            float r = 5.0f;
            ImVec2 diamond[4] = {
                ImVec2(posX, centerY - r),
                ImVec2(posX + r, centerY),
                ImVec2(posX, centerY + r),
                ImVec2(posX - r, centerY)
            };
            drawList->AddConvexPolyFilled(diamond, 4, IM_COL32(255, 230, 0, 255));
            drawList->AddPolyline(diamond, 4, IM_COL32(0, 0, 0, 255), ImDrawFlags_Closed, 1.5f);
        }
    }


    //再生中の時間進行処理
    if (isPlaying_ && duration > 0.0f)
    {
        inOutTime += deltaTime * playbackSpeed_;

        if (isLoop_)
        {
            if (inOutTime > duration)inOutTime = std::fmod(inOutTime, duration);
            if (inOutTime < 0.0f)inOutTime += duration;
        }
        else
        {
            if (inOutTime >= duration)
            {
                inOutTime = duration;
                isPlaying_ = false;
            }
            else if (inOutTime <= 0.0f)
            {
                inOutTime = 0.0f;
                isPlaying_ = false;
            }
        }
    }
    ImGui::End();
    return action;
}