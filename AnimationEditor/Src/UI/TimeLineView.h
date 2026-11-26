#pragma once
#include<vector>
#include"../Edit/EditorState.h"

/// <summary>
/// タイムラインウィンドウ
/// クリップの選択、再生操作、ブレンド設定、シークバーとキーマーカーを表示する
/// </summary>
class TimeLineView
{
public:
	TimeLineView(void) = default;
	~TimeLineView(void) = default;

	/// <summary>
	/// タイムラインを描画し、再生時刻や再生状態を更新する
	/// </summary>
	/// <param name="state">エディタの状態(クリップ・再生状態・ブレンド設定などを読み書きする)</param>
	/// <param name="deltaTime">前のフレームからの経過時間(秒)</param>
	/// <returns>キーを変更したか(ポーズを計算し直す必要があるか)</returns>
	bool Draw(EditorState& state, float deltaTime);

private:
	/// <summary>
	/// シークバーとキーマーカー
	/// 左クリック:その時刻へ移動 左ドラッグ:キーを移す 右クリック:キーを消す
	/// </summary>
	/// <returns>キーを変更したか</returns>
	bool DrawSeekBar(EditorState& state, AnimationClip& clip, const std::vector<float>& keyTimes);

	/// <summary>
	/// 区間の速度変更(In点・Out点・倍率を決めて、元の動きごと速さを変える)
	/// </summary>
	/// <returns>変更したか(ポーズを計算し直す必要があるか)</returns>
	bool DrawSpeedRangeControls(EditorState& state, AnimationClip& clip);

	//---------- 区間の速度変更 ----------
	float speedRangeIn_ = -1.0f;	//In点の時刻(-1:未設定。未設定ならクリップの先頭)
	float speedRangeOut_ = -1.0f;	//Out点の時刻(-1:未設定。未設定ならクリップの終端)
	float rangeSpeed_ = 0.5f;		//区間の速度の倍率
	bool useSpeedCurve_ = false;	//区間の中の緩急をベジェ曲線で付けるか
	BezierHandles speedCurve_ = { 0.42f, 0.0f, 0.58f, 1.0f };	//区間の中の緩急(横:新しい時間 縦:元の動きの進み具合)
	int speedRangeClip_ = -1;		//区間を決めたクリップ(クリップが替わったら区間を消す)
	//キーのドラッグの状態
	struct KeyDrag
	{
		bool active = false;	//ドラッグ中か
		int clipIndex = -1;		//ドラッグを始めたクリップ
		int boneIndex = -1;		//対象のボーン(-1:全ボーン)
		float fromTime = 0.0f;	//元の時刻
		float toTime = 0.0f;	//移動先の時刻
	};
	KeyDrag keyDrag_;
};