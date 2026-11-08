#pragma once
#include <string>
#include <vector>
#include <Windows.h>
#include "../Animation/AnimationClip.h"

class TimeLineView
{
public:
    TimeLineView() = default;
    ~TimeLineView() = default;

    // メニューバーとタイムラインを描画
    // 返り値: メニューからファイル追加が要求された場合、選択されたファイルパス（空ならなし）
    std::string Draw(
        HWND hWnd,
        const std::vector<AnimationClip>& clips,
        int& currentClipIndex,
        float& inOutTime,
        float deltaTime,
        bool& outEnableBlend,
        int& outTargetClipIndex,
        float& outBlendWeight,
        int selectedBoneIndex);

    bool IsPlaying() const { return isPlaying_; }
    void SetPlaying(bool play) { isPlaying_ = play; }
    float GetPlaybackSpeed() const { return playbackSpeed_; }

private:
    bool isPlaying_ = true;
    bool isLoop_ = true;
    float playbackSpeed_ = 1.0f;
};