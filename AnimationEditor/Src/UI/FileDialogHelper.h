#pragma once
#include<Windows.h>
#include<commdlg.h>
#include<string>

/// <summary>
/// Windows基準のファイルを開くダイアログを表示し、指定されたFBXファイルパスを返す
/// </summary>
/// <param name="hWnd">親ウィンドウ</param>
/// <returns>選択されたファイルのフルパス</returns>
inline std::string OpenFbxFileDialog(HWND hWnd)
{
    //選択されたファイルのフルパスを受け取る文字列(MAX＿PATH＝260文字)
	char fileName[MAX_PATH] = "";
	//ファイル選択ダイアルぐの設定情報を格納
    OPENFILENAME ofn = {};
	ofn.lStructSize = sizeof(OPENFILENAME);
    //このダイアルぐを所有する親ウィンドウのハンドル
	ofn.hwndOwner = hWnd;
    //ダイアログ右下に表示する拡張子フィルタ
	ofn.lpstrFilter = "FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
    //ダイアルぐで選択されたファイルパス文字列が書き込まれる出力先バッファのポインタ
	ofn.lpstrFile = fileName;
	ofn.nMaxFile = MAX_PATH;
    //ダイアルグ動作オプションフラグの組み合わせ
   // OFN_FILEMUSTEXIST : 実際にディスク上に存在するファイルのみ選択を許可する
    // OFN_NOCHANGEDIR   : ダイアログでフォルダを移動しても、アプリのカレントディレクトリ（相対パスの基準）を変えない
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    //Windows標準のファイルを開くモーダルダイアルグを表示
	if (GetOpenFileName(&ofn))
	{
		return std::string(fileName);
	}
	return "";
}

/// <summary>
/// Windows標準の名前を付けて保存ダイアログを表示し、保存先のFBXファイルパスを返す
/// </summary>
/// <param name="hWnd">親ウィンドウのハンドル</param>
/// <returns>指定された保存先のフルパス</returns>
inline std::string SaveFbxFileDialog(HWND hWnd)
{
    //保存ダイアログを開いた際、ファイル名入力欄にあらかじめ入ってる初期ファイル名
    char fileName[MAX_PATH] = "ExportedMotion.fbx";
    //保存ダイアログの設定情報を格納する構造体
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    //親ウィンドウのハンドル
    ofn.hwndOwner = hWnd;
    //保存形式のフィルタ定義
    ofn.lpstrFilter = "FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
    //ユーザーが入力、指定した保存先フルパスが格納されるバッファ
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
   //ユーザーが拡張しを入力せずファイル名だけを書いた場合に自動補完される規定の拡張し
    ofn.lpstrDefExt = "fbx";
    // 保存ダイアログの動作オプションフラグの組み合わせ:
    // OFN_OVERWRITEPROMPT : すでに同名のファイルが存在する場合、「上書きしますか？」の確認ダイアログを自動表示する
    // OFN_NOCHANGEDIR     : フォルダ移動時にアプリ側の作業ディレクトリ（相対パス基準）がズレるのを防ぐ
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn))
    {
        return std::string(fileName);
    }
    return "";
}