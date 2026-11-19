#pragma once
#pragma once

/// エディタ全体で共有する定数
/// 複数のファイルで同じ意味の数値を使うものだけをここに置く
/// (1つのファイルでしか使わない数値は、そのファイルの先頭に置く)
namespace EditorConst
{
	// 2つのキーを「同じ時刻」とみなす許容誤差(秒)
	// キーの検索・上書き・削除・タイムラインの表示など、キーの時刻比較はすべてこれを使う
	constexpr float kKeyTimeEpsilon = 0.001f;

	// アニメーションをサンプリングするフレームレート
	// FBX読み込み時のベイク間隔と、Shift+←→のフレーム送りに使う
	constexpr float kSampleFrameRate = 30.0f;

	// 1フレームの長さ(秒)
	constexpr float kFrameDuration = 1.0f / kSampleFrameRate;
}