#pragma once
#include<vector>
#include<string>
#include"../Animation/Skeleton.h"
#include"../Animation/Pose.h"
#include"../Animation/AnimationClip.h"


class InspectorView
{
public:
    InspectorView(void) = default;
    ~InspectorView(void) = default;

    /// <summary>
    ///インスペクターウィンドウを描画し、ボーン選択および姿勢、キーフレーム編集を行う 
    /// </summary>
    /// <param name="skeleton">ボーンの親子関係や初期バインドポーズを参照するためのスケルトンデータ</param>
    /// <param name="inOutPose">現在表示、編集中のローカルポーズ</param>
    /// <param name="currentClip">現在編集対象のアニメーションクリップ</param>
    /// <param name="currentTime">タイムライン上の現在再生時刻</param>
    /// <returns>このフレームでキーフレームの登録または区間設置が実行されたかどうか</returns>
    bool Draw(const Skeleton& skeleton, Pose& inOutPose, AnimationClip* currentClip, float currentTime);

    //現在インスペクター上で選択されているボーンのインデクスを取得する
    int GetSelectedBoneIndex(void)const { return selectedBoneIndex_; }
private:
	void DrawBoneNode(int boneIndex, const Skeleton& skeleton);

    int selectedBoneIndex_ = -1;    // 現在選択されているボーンID
    float rangeStartTime_ = 0.0f;  // 区間置換の開始時刻
    float rangeEndTime_ = 0.5f;  // 区間置換の終了時刻
    Transform rangeStartTrs_;         // In点に記録しておくポーズ
    bool hasRangeStartPose_ = false; // In点のポーズがすでに記録済みかどうかのフラグ
};

