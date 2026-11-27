#pragma once

struct ImFont;

/// <summary>
/// 操作方法の一覧ウィンドウ(Tabで出し入れする)
/// </summary>
namespace ControlsHelp
{
	/// <summary>
	/// 操作方法の一覧を描く
	/// </summary>
	/// <param name="inOutOpen">表示するか(ウィンドウの×で閉じるとfalseになる)</param>
	/// <param name="japaneseFont">日本語のフォント(nullptrなら英語で表示する)</param>
	void Draw(bool& inOutOpen, ImFont* japaneseFont);
}