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