#pragma once
#include"../Animation/Easing.h"

/// <summary>
/// ベジェ曲線を編集するグラフ(インスペクターの補間とタイムラインの速度変更で使う)
/// 制御点のドラッグ・プリセットのボタン・数値の入力欄をまとめて出す
/// </summary>
namespace BezierCurveWidget
{
	//よく使う曲線(ボタンで選べるもの)
	struct Preset
	{
		const char* name;
		BezierHandles handles;
	};

	//グラフの設定
	struct Options
	{
		float valueMin = 0.0f;			//縦軸の範囲(制御点のyもこの範囲に収める)
		float valueMax = 1.0f;
		float height = 150.0f;			//グラフの高さ
		const Preset* presets = nullptr;	//プリセット(無ければnullptr)
		int presetCount = 0;
	};

	/// <summary>
	/// グラフを描き、操作されたら曲線を書き換える
	/// </summary>
	/// <param name="id">ImGuiのID(同じ画面に2つ出すときに区別するため)</param>
	/// <param name="inOutHandles">曲線(操作されると書き換わる)</param>
	/// <param name="outEditStarted">このフレームで編集が始まったか(ドラッグ開始・プリセットを押した。Undoの履歴を積むのに使う)</param>
	/// <returns>曲線を書き換えたか</returns>
	bool Draw(const char* id, BezierHandles& inOutHandles, const Options& options, bool& outEditStarted);
}