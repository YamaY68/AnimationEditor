#pragma once
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
	void Draw(EditorState& state, float deltaTime);
};