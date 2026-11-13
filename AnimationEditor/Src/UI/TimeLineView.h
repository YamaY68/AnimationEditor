#pragma once
#include<Windows.h>
#include<string>
#include<vector>
#include"../Animation/AnimationClip.h"

enum class MenuActionType
{
    None,           
    AddAnimation,   
    ExportFbx
};

class TimeLineView
{
public:
    TimeLineView(void) = default;
    ~TimeLineView(void) = default;
    
    /// <summary>
    /// 上部メインメニュー及びタイムラインウィンドウを描画、再生時間や操作状態を更新
    /// </summary>
    /// <param name="hWnd"><ファイルダイアログの親ウィンドウハンドル/param>
    /// <param name="clips">読み込まれているアニメーションクリップ一覧</param>
    /// <param name="currentClipIndex">再生、編集中のクリップインデックス</param>
    /// <param name="inOutTime">現在の再生時刻</param>
    /// <param name="deltaTime">前フレームからの経過時間</param>
    /// <param name="outEnableBlend">ブレンド再生が有効かどうかのフラグ</param>
    /// <param name="outTargetClipIndex">ブレンド先クリップのインデックス</param>
    /// <param name="outBlendWeight">ブレンドウェイト0~1.0f</param>
    /// <param name="selectedBoneIndex">マーカー表示対象の選択中のボーンID</param>
    /// <param name="outSelectedFilePath">ダイアログかあｒ選択されたＦＢＸファイルパスの出力先</param>
    /// <returns>メニューバーから実行されたアクション種別</returns>
    MenuActionType Draw(
        HWND hWnd,
        const std::vector<AnimationClip>& clips,int& currentClipIndex,
        float& inOutTime,float deltaTime,
        bool& outEnableBlend,int& outTargetClipIndex,
        float& outBlendWeight,int selectedBoneIndex,
        std::string& outSelectedFilePath
    );

    // 現在再生中かどうかを取得する
    bool IsPlaying(void) const { return isPlaying_; }

    //再生、一時停止状態を切り替える
    void SetPlaying(bool play) { isPlaying_ = play; }

    //再生速度倍率を取得
    float GetPlaybackSpeed(void) const { return playbackSpeed_; }

private:
    bool isPlaying_ = true;
    bool isLoop_ = true;
    float playbackSpeed_ = 1.0f;

};

