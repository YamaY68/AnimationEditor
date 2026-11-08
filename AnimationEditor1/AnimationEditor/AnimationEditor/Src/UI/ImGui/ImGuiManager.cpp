#include "ImGuiManager.h"
#include <DxLib.h>
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <shellapi.h>
#include <vector>
#include <string>

// ドロップされたファイルパスの保持用バッファ
static std::vector<std::string> s_DroppedFiles;

// imgui_impl_win32.cpp 内のプロシージャハンドラ
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// DxLibのプロシージャをサブクラス化してImGuiの入力 & ドロップを拾う
static WNDPROC s_OriginalWndProc = nullptr;
static LRESULT CALLBACK SubclassWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // ★ WM_DROPFILES のハンドリング
    if (msg == WM_DROPFILES)
    {
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        UINT fileCount = DragQueryFileA(hDrop, 0xFFFFFFFF, nullptr, 0);

        for (UINT i = 0; i < fileCount; ++i)
        {
            char pathBuffer[MAX_PATH];
            if (DragQueryFileA(hDrop, i, pathBuffer, MAX_PATH) > 0)
            {
                s_DroppedFiles.push_back(std::string(pathBuffer));
            }
        }
        DragFinish(hDrop);
        return 0;
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
    {
        return true;
    }
    return CallWindowProc(s_OriginalWndProc, hWnd, msg, wParam, lParam);
}

bool ImGuiManager::Initialize(HWND hWnd)
{
    if (isInitialized_) return true;

    ID3D11Device* device = static_cast<ID3D11Device*>(const_cast<void*>(GetUseDirect3D11Device()));
    ID3D11DeviceContext* context = static_cast<ID3D11DeviceContext*>(const_cast<void*>(GetUseDirect3D11DeviceContext()));

    if (!device || !context)
    {
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    if (!ImGui_ImplWin32_Init(hWnd))
    {
        return false;
    }

    if (!ImGui_ImplDX11_Init(device, context))
    {
        ImGui_ImplWin32_Shutdown();
        return false;
    }

    // ウィンドウプロシージャをフック
    s_OriginalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SubclassWndProc)));

    // ★ ウィンドウへのファイルドラッグ＆ドロップ受け入れを有効化
    DragAcceptFiles(hWnd, TRUE);

    isInitialized_ = true;
    return true;
}

void ImGuiManager::Finalize()
{
    if (!isInitialized_) return;

    HWND hWnd = reinterpret_cast<HWND>(GetMainWindowHandle());
    if (hWnd)
    {
        DragAcceptFiles(hWnd, FALSE);
        if (s_OriginalWndProc)
        {
            SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_OriginalWndProc));
            s_OriginalWndProc = nullptr;
        }
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    s_DroppedFiles.clear();
    isInitialized_ = false;
}

void ImGuiManager::BeginFrame()
{
    if (!isInitialized_) return;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::EndFrame()
{
    if (!isInitialized_) return;

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

bool ImGuiManager::WantCaptureMouse() const
{
    if (!isInitialized_) return false;
    return ImGui::GetIO().WantCaptureMouse;
}

bool ImGuiManager::WantCaptureKeyboard() const
{
    if (!isInitialized_) return false;
    return ImGui::GetIO().WantCaptureKeyboard;
}

std::vector<std::string> ImGuiManager::PopDroppedFiles()
{
    std::vector<std::string> files = std::move(s_DroppedFiles);
    s_DroppedFiles.clear();
    return files;
}