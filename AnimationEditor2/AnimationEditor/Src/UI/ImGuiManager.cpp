#include "ImGuiManager.h"
#include<shellapi.h>
#include<DxLib.h>
#include<imgui.h>
#include<imgui_impl_win32.h>
#include<imgui_impl_dx11.h>
#include<d3d11.h>

//ImGui側で定義されているWin32ウィンドウプロシージャハンドラの外部定義
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

//ドロップされたファイルパスを一時蓄積するファイル内静的キュー
static std::vector<std::string>droppedFiles_;

/// <summary>
/// Dxライブラリに登録するカスタムウィンドウプロシージャ
/// </summary>
/// <param name="hWnd">メッセージを受け取ったウィンドウハンドル</param>
/// <param name="msg">メッセージID</param>
/// <param name="wParam">メッセージの追加パラメータ１</param>
/// <param name="lParam">メッセージの追加パラメータ２</param>
/// <returns>メッセージを処理した場合は０、または処理結果、DXLib側に流す場合は０</returns>
static LRESULT CALLBACK CustomWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    //ImGuiの入力バックエンドへメッセージを渡す　UI上のクリックや文字入力を処理
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    {
        return true;
    }
    //エクスプローラーからウィンドウへふぁいるがドラッグ＆ドロップされた場合の処理
    if (msg == WM_DROPFILES)
    {
        //ドロップ操作のハンドルを取得
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        //第二引数に0xFFFFFFFFを渡すことで同時にドロップされたファイルの総数を取得
        UINT fileCount = DragQueryFileA(hDrop, 0xFFFFFFFF, nullptr, 0);
        //ドロップされた全ファイルについて１つずつフルパスを取り出す
        for (UINT i = 0; i < fileCount; ++i)
        {
            char filePath[MAX_PATH] = "";
            //i番目のファイルパスを文字列バッファへコピー
            if (DragQueryFileA(hDrop, i, filePath, MAX_PATH) > 0)
            {
                //取り出したパスをキュー側へ追加
                //メインループ側でPopDroppedFilesで回収
                droppedFiles_.emplace_back(filePath);
            }
        }
        //Windowsが確保したドロップ情報のメモリを解放
        DragFinish(hDrop);
        return 0;
    }

    return 0;
}

bool ImGuiManager::Initialize(HWND hWnd)
{
    if (isInitialized_)return true;

    //ImGuiのバージョンチェックと内部コンテキストの生成
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    //ImGuiの基本設定
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();

    //DxLibが内部で作成、保持しているDirect3D11のデバイスとデバイスコンテキストを取得
    ID3D11Device* device = const_cast<ID3D11Device*>(
        static_cast<const ID3D11Device*>(GetUseDirect3D11Device()));
    ID3D11DeviceContext* context = const_cast<ID3D11DeviceContext*>(
        static_cast<const ID3D11DeviceContext*>(GetUseDirect3D11DeviceContext()));

    //DirectX11の取得に失敗した場合は初期化失敗
    if (!device || !context)
    {
        ImGui::DestroyContext();
        return false;
    }

    //Win32バックエンドとDirectX11バックエンドを初期化
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(device, context);

    //DxLibにカスタムウィンドウプロシージャを登録(マウスホイールや文字入力をImGuiへ届けるため)
    SetHookWinProc(CustomWndProc);

    //OSに対してこのウィンドウがファイルのドラッグ＆ドロップを受け入れることを宣言
    DragAcceptFiles(hWnd, TRUE);

    isInitialized_ = true;
    return true;
}

void ImGuiManager::Finalize(void)
{
    if (!isInitialized_)return;
    //初期化の逆順で
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    isInitialized_ = false;
}

void ImGuiManager::BeginFrame(void)
{
    if (!isInitialized_)return;
    //DirectX１１とWin32のフレーム状態の更新のあとImGui本体の新規フレームを開始
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::EndFrame(void)
{
    if (!isInitialized_)return;
    //UI、ウィジェットの頂点、描画コマンドを生成
    ImGui::Render();
    //DxLibの３D描画で設定されたシェーダーやブレンドステートをリセット
    //DxLibで３Dモデルを描画した直後のシェーダーやZバッファをリセット
    RefreshDxLibDirect3DSetting();
    //生成された描画データをDirectX11のバックバッファへ実際に描画
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool ImGuiManager::WantCaptureMouse(void)const
{
    if(!isInitialized_)return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiManager::WantCaptureKeyboard(void)const
{
    if (!isInitialized_)return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}

std::vector<std::string>ImGuiManager::PopDroppedFiles(void)
{
    std::vector<std::string>result = std::move(droppedFiles_);
    droppedFiles_.clear();
    return result;
}