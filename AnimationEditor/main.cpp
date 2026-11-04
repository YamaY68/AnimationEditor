#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// FBX SDK の DLL 用インポートライブラリ
#include <fbxsdk.h>
#pragma comment(lib, "libfbxsdk.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "wininet.lib")

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_win32.h"
#include "ImGui/imgui_impl_dx11.h"

#include "Renderer/Renderer.h"
#include "Mesh/Mesh.h"
#include "Camera/Camera.h"

using namespace DirectX;
using Microsoft::WRL::ComPtr;

// ImGui用Win32メッセージハンドラーの宣言
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

constexpr UINT WINDOW_WIDTH = 1280;
constexpr UINT WINDOW_HEIGHT = 720;
constexpr const wchar_t* kWindowClassName = L"DX11ToolWindowClass";



// ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }
    switch (msg) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
    // 1. ウィンドウクラスの登録
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = kWindowClassName;
    RegisterClassExW(&wc);

    RECT wr = { 0, 0, static_cast<LONG>(WINDOW_WIDTH), static_cast<LONG>(WINDOW_HEIGHT) };
    AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

    // 2. ウィンドウ生成
    HWND hWnd = CreateWindowExW(
        0,
        kWindowClassName,
        L"Animation Editor",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        wr.right - wr.left, wr.bottom - wr.top,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hWnd) return -1;

    ShowWindow(hWnd, nCmdShow);

    // 3. レンダラーの初期化 (DX11デバイス・スワップチェーン・パイプライン構築)
    Renderer renderer;
    if (!renderer.Init(hWnd, WINDOW_WIDTH, WINDOW_HEIGHT)) {
        MessageBoxW(hWnd, L"Rendererの初期化に失敗しました。", L"Error", MB_OK);
        return -1;
    }

    // 4. カメラの初期化
    float aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);
    // farZ を 100.0f から 2000.0f に拡大
    Camera camera(DirectX::XMConvertToRadians(45.0f), aspect, 0.1f, 2000.0f);

    Mesh modelMesh;
    if (!modelMesh.LoadFBX(renderer.GetDevice(), "Data/Model/SowrdMan.fbx")) {
        // ここに入っているか確認
        OutputDebugStringA("FBX Load Failed -> Fallback to Cube\n");
        modelMesh.CreateCube(renderer.GetDevice());
    }
    else {
        OutputDebugStringA("FBX Load Success!\n");
    }

    // 6. ImGui 初期化 (Rendererの持つデバイスとコンテキストを共有)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hWnd);
    ImGui_ImplDX11_Init(renderer.GetDevice(), renderer.GetContext());

    // 制御パラメータ
    float modelPos[3] = { 0.0f, 0.0f, 0.0f };
    float modelRot[3] = { 0.0f, 0.0f, 0.0f };
    float modelScale[3] = { 1.0f, 1.0f, 1.0f }; // スケール追加
    float clearColor[4] = { 0.2f, 0.2f, 0.22f, 1.0f };

    // メインループ
    MSG msg{};
    while (msg.message != WM_QUIT) {
        if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        else {
            // ImGuiフレーム開始
            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            // 操作用UI
            {
                ImGui::Begin("Tool Controller");
                ImGui::Text("FPS: %.1f (%.3f ms/frame)", io.Framerate, 1000.0f / io.Framerate);
                ImGui::Separator();
                ImGui::ColorEdit4("Clear Color", clearColor);
                ImGui::DragFloat3("Position", modelPos, 0.1f);
                ImGui::DragFloat3("Rotation", modelRot, 1.0f);
                ImGui::DragFloat3("Scale", modelScale, 0.01f, 0.0001f, 100.0f);
                ImGui::Text("Index Count: %u", modelMesh.GetIndexCount());
                ImGui::End();
            }

            // 描画準備
            renderer.BeginFrame(clearColor);

            DirectX::XMMATRIX matScale = DirectX::XMMatrixScaling(modelScale[0], modelScale[1], modelScale[2]);
            DirectX::XMMATRIX matRot = DirectX::XMMatrixRotationRollPitchYaw(
                DirectX::XMConvertToRadians(modelRot[0]),
                DirectX::XMConvertToRadians(modelRot[1]),
                DirectX::XMConvertToRadians(modelRot[2])
            );
            DirectX::XMMATRIX matTrans = DirectX::XMMatrixTranslation(modelPos[0], modelPos[1], modelPos[2]);
            DirectX::XMMATRIX world = matScale * matRot * matTrans;

            // 定数バッファ転送
            renderer.SetTransform(world, camera.GetViewMatrix(), camera.GetProjMatrix());

            // メッシュ描画
            modelMesh.Draw(renderer.GetContext());

            // ImGui描画
            ImGui::Render();
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

            // 画面表示
            renderer.EndFrame();
        }
    }

    // 解放処理
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    DestroyWindow(hWnd);
    UnregisterClassW(kWindowClassName, hInstance);

    return static_cast<int>(msg.wParam);
}