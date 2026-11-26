#include "ImGuiManager.h"
#include<DxLib.h>
#include<imgui.h>
#include<imgui_impl_win32.h>
#include<imgui_impl_dx11.h>
#include<d3d11.h>
#include<ImGuizmo.h>

//ImGui側で定義されているWin32ウィンドウプロシージャハンドラの外部定義
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


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
    //ImGuiの入力バックエンドへメッセージを渡す(UI上のクリックや文字入力を処理)
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    {
        return true;
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
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGuizmo::BeginFrame();	
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