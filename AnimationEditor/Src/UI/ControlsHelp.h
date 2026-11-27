#pragma once

/// <summary>
/// 操作方法の一覧ウィンドウ(Tabで出し入れする)
/// (ImGuiのフォントに日本語が入っていないので、表示は英語)
/// </summary>
namespace ControlsHelp
{
	/// <summary>
	/// 操作方法の一覧を描く
	/// </summary>
	/// <param name="inOutOpen">表示するか(ウィンドウの×で閉じるとfalseになる)</param>
	void Draw(bool& inOutOpen);
}