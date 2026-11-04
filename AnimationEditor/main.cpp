#include<Windows.h>
#include<d3d11.h>
#include<wrl/client.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
//#include <fbxsdk.h>
//
//#if defined(_DEBUG)
//#pragma comment(lib, "libfbxsdk-md.lib")
//#else
//#pragma comment(lib, "libfbxsdk-md.lib")
//#endif

#include"imgui.h"
#include"imgui_impl_win32.h"
#include"imgui_impl_dx11.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM mParam, LPARAM lParam);

using Microsoft::WRL::ComPtr;

constexpr UINT WINDOW_WIDTH = 1280;
constexpr UINT WINDOW_HEIGHT = 720;

//Dirextx11　関連オブジェクト
struct D3D11Context {
	ComPtr<ID3D11Device>device;
	ComPtr<ID3D11DeviceContext>context;
	ComPtr<IDXGISwapChain>swapChain;
	ComPtr<ID3D11RenderTargetView>rtv;
	ComPtr<ID3D11DepthStencilView>dsv;
	D3D11_VIEWPORT viewport{};
};

//ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
		return true;
	}
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}

//DX11 初期化関数
bool InitD3D11(HWND hWnd, D3D11Context& d3d)
{
	DXGI_SWAP_CHAIN_DESC scd{};
	scd.BufferCount = 1;
	scd.BufferDesc.Width = WINDOW_WIDTH;
	scd.BufferDesc.Height = WINDOW_HEIGHT;
	scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.BufferDesc.RefreshRate.Numerator = 60;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.OutputWindow = hWnd;
	scd.SampleDesc.Count = 1;
	scd.SampleDesc.Quality = 0;
	scd.Windowed = TRUE;

	UINT createDeviceFlags = 0;
#ifdef DEBUG
	//デバッグビルド時はDX警告ログを有効か
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	D3D_FEATURE_LEVEL featuerLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	D3D_FEATURE_LEVEL featureLevel;

	//デバイス、コンテキスト、スワップチェーンの生成
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createDeviceFlags,
		featuerLevels,
		1,
		D3D11_SDK_VERSION,
		&scd,
		d3d.swapChain.GetAddressOf(),
		d3d.device.GetAddressOf(),
		&featureLevel,
		d3d.context.GetAddressOf()
	);
	if (FAILED(hr))return false;

	//レンダーターゲットビューの生成
	ComPtr<ID3D11Texture2D>backBuffer;
	hr = d3d.swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
	if (FAILED(hr))return false;

	hr = d3d.device->CreateRenderTargetView(backBuffer.Get(), nullptr, d3d.rtv.GetAddressOf());
	if (FAILED(hr))return false;

	//震度ステンシルテクスチャとビュー(DSV)の作成　３D描画用
	D3D11_TEXTURE2D_DESC depthDesc{};
	depthDesc.Width = WINDOW_WIDTH;
	depthDesc.Height = WINDOW_HEIGHT;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	ComPtr<ID3D11Texture2D>depthStencilBuffer;
	hr = d3d.device->CreateTexture2D(&depthDesc, nullptr, depthStencilBuffer.GetAddressOf());
	if (FAILED(hr)) return false;

	hr = d3d.device->CreateDepthStencilView(depthStencilBuffer.Get(), nullptr, d3d.dsv.GetAddressOf());
	if (FAILED(hr))return false;

	//ビューポート設定
	d3d.viewport.TopLeftX = 0.0f;
	d3d.viewport.TopLeftY = 0.0f;
	d3d.viewport.Width = static_cast<FLOAT>(WINDOW_WIDTH);
	d3d.viewport.Height = static_cast<FLOAT>(WINDOW_HEIGHT);
	d3d.viewport.MinDepth = 0.0f;
	d3d.viewport.MaxDepth = 1.0f;

	return true;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow) {
	// 1. ウィンドウクラスの登録
	const wchar_t* CLASS_NAME = L"DX11ToolWindowClass";
	WNDCLASSEX wc{};
	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.lpszClassName = CLASS_NAME;
	RegisterClassEx(&wc);

	RECT wr = { 0,0,static_cast<LONG>(WINDOW_WIDTH),static_cast<LONG>(WINDOW_HEIGHT) };
	AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

	//ウィンドウ生成
	HWND hWnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"Animation Editor",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		wr.right - wr.left, wr.bottom - wr.top,
		nullptr, nullptr, hInstance, nullptr);
	if (!hWnd)return -1;

	ShowWindow(hWnd, nCmdShow);

	//DirectX11初期化
	D3D11Context d3d;
	if (!InitD3D11(hWnd, d3d)) {
		MessageBox(hWnd, L"DirectX11の初期化に失敗", L"Error", MB_OK);
		return -1;
	}

	//ImGui初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(d3d.device.Get(), d3d.context.Get());

	//画面クリア
	float clearColor[4] = { 0.2f,0.2f,0.22f,1.0f };
	//メインループ
	MSG msg{};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			//ImGuiフレーム開始
			ImGui_ImplDX11_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			//テスト用UIウィンドウの構築
			{
				ImGui::Begin("Tool Controller");
				ImGui::Text("DirectX11+ImGui Initialized");
				ImGui::ColorEdit4("Clear Color", clearColor);
				ImGui::End();
			
				ImGui::ShowDemoWindow();
			}

			//描画処理
			//パイプラインにレンダーターゲットと深度バッファ、ビューポートを設定
			d3d.context->OMSetRenderTargets(1, d3d.rtv.GetAddressOf(), d3d.dsv.Get());
			d3d.context->RSSetViewports(1, &d3d.viewport);
			d3d.context->ClearRenderTargetView(d3d.rtv.Get(), clearColor);
			d3d.context->ClearDepthStencilView(d3d.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			//ImGuiの描画コマンド
			ImGui::Render();
			ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
			
			//メッシュ描画等



			//画面フリップ
			d3d.swapChain->Present(1, 0);
		}
	}

	//終了処理
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	return static_cast<int>(msg.wParam);
}

