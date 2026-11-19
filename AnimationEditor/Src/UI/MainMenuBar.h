#pragma once
#include<Windows.h>
#include<string>

/// <summary>
/// メニューで選ばれた操作の種類
/// </summary>
enum class MenuAction
{
	None,			//何も選ばれていない
	AddAnimation	//アニメーションFBXを追加する
};

/// <summary>
/// メニューの操作結果
/// </summary>
struct MenuResult
{
	MenuAction action = MenuAction::None;	//選ばれた操作
	std::string filePath;					//ダイアログで選ばれたファイル(操作に必要な場合だけ)
};

/// <summary>
/// 画面上部のメインメニューバー
/// </summary>
namespace MainMenuBar
{
	/// <summary>
	/// メニューバーを描画し、選ばれた操作を返す
	/// </summary>
	/// <param name="hWnd">ファイルダイアログの親ウィンドウ</param>
	MenuResult Draw(HWND hWnd);
}