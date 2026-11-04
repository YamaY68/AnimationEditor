#include "Renderer.h"
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")


bool Renderer::Init(HWND hWnd, UINT width, UINT height)
{   
    if (!CreateDeviceAndSwapChain(hWnd, width, height))return false;
    if (!CreatePipeline())return false;
    return true;
}

bool Renderer::CreateDeviceAndSwapChain(HWND hWnd, UINT width, UINT height) 
{
    //デバイスとスワップチェーンの作成
    //バッファの数、幅、高さ、フォーマット、リフレッシュレート、
    //使用方法、出力ウィンドウ、サンプル数、ウィンドウモードを設定
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 1;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hWnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;

    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    //D3D_FEATURE_LEVEL11_0を指定してデバイスを作成
    D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
    D3D_FEATURE_LEVEL featureLevel;
    //デバイスとスワップチェーンの作成
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        featureLevels,
        1,
        D3D11_SDK_VERSION,
        &scd,
        swapChain_.GetAddressOf(),
        device_.GetAddressOf(),
        &featureLevel,
        context_.GetAddressOf()
    );
    if (FAILED(hr)) return false;

    // レンダーターゲットビュー生成
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    //バックバッファを取得してレンダーターゲットビューを作成
    hr = swapChain_->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backBuffer.GetAddressOf()));
    if (FAILED(hr)) return false;
    hr = device_->CreateRenderTargetView(backBuffer.Get(), nullptr, rtv_.GetAddressOf());
    if (FAILED(hr)) return false;

    // 深度ステンシルバッファ生成
	//バッファの幅、高さ、ミップマップレベル、配列サイズ、
    //フォーマット、サンプル数、使用方法、バインドフラグを設定
    D3D11_TEXTURE2D_DESC depthDesc{};
    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.SampleDesc.Quality = 0;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    //深度ステンシルバッファを作成し、深度ステンシルビューを作成
    Microsoft::WRL::ComPtr<ID3D11Texture2D> depthBuffer;
    hr = device_->CreateTexture2D(&depthDesc, nullptr, depthBuffer.GetAddressOf());
    if (FAILED(hr)) return false;
    hr = device_->CreateDepthStencilView(depthBuffer.Get(), nullptr, dsv_.GetAddressOf());
    if (FAILED(hr)) return false;

    // ビューポート設定
	//ビューポートの左上の座標、幅、高さ、最小深度、最大深度を設定
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.Width = static_cast<FLOAT>(width);
    viewport_.Height = static_cast<FLOAT>(height);
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    return true;
}

bool Renderer::CreatePipeline(void)
{
    //シェーダーのコンパイルと作成
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob, errorBlob;
    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#endif

    // 頂点シェーダーコンパイル
    HRESULT hr = D3DCompileFromFile(
        L"BasicShader/BasicShader.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VSMain", "vs_5_0", flags, 0,
        vsBlob.GetAddressOf(), errorBlob.GetAddressOf()
    );
    if (FAILED(hr)) {
        if (errorBlob) {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
            MessageBoxA(nullptr, static_cast<char*>(errorBlob->GetBufferPointer()), "Shader Compile Error", MB_OK);
        }
        else {
            MessageBoxW(nullptr, L"BasicShader/BasicShader.hlsl が見つかりません。カレントディレクトリを確認してください。", L"File Not Found", MB_OK);
        }
        return false;
    }

    // ピクセルシェーダーコンパイル
    hr = D3DCompileFromFile(
        L"BasicShader/BasicShader.hlsl", nullptr, nullptr, "PSMain", "ps_5_0", flags, 0,
        psBlob.GetAddressOf(), errorBlob.GetAddressOf()
    );
    if (FAILED(hr)) {
        if (errorBlob) OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        return false;
    }

    if (FAILED(device_->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, vs_.GetAddressOf()))) {
        return false;
    }
    if (FAILED(device_->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, ps_.GetAddressOf()))) {
        return false;
    }

    // 入力レイアウト定義
// 入力レイアウト定義 (Mesh.h の Vertex 構造体のメモリ配置と完全一致させる)
    D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,                            D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    if (FAILED(device_->CreateInputLayout(layoutDesc, _countof(layoutDesc), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), inputLayout_.GetAddressOf()))) {
        return false;
    }
    // カリング無効（両面描画）ステートの生成
    D3D11_RASTERIZER_DESC rsDesc{};
    rsDesc.FillMode = D3D11_FILL_SOLID;
    rsDesc.CullMode = D3D11_CULL_NONE; // 両面を描画
    rsDesc.DepthClipEnable = FALSE;    // 視錐台外クリップも一旦無効化
    if (FAILED(device_->CreateRasterizerState(&rsDesc, rasterizerState_.GetAddressOf()))) {
        return false;
    }

    // 深度テスト無効化ステート（強制的に最前面描画）
    D3D11_DEPTH_STENCIL_DESC dsDesc{};
    dsDesc.DepthEnable = FALSE; // 一旦深度テストをオフにして確実に画面に出す
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    if (FAILED(device_->CreateDepthStencilState(&dsDesc, depthStencilState_.GetAddressOf()))) {
        return false;
    }
    // 定数バッファ生成
    D3D11_BUFFER_DESC cbDesc{};
    cbDesc.ByteWidth = sizeof(ConstantBuffer);
    cbDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(device_->CreateBuffer(&cbDesc, nullptr, constantBuffer_.GetAddressOf()))) {
        return false;
    }

    return true;
}

void Renderer::BeginFrame(const float clearColor[4]) {
    context_->OMSetRenderTargets(1, rtv_.GetAddressOf(), dsv_.Get());
    context_->RSSetViewports(1, &viewport_);
    context_->ClearRenderTargetView(rtv_.Get(), clearColor);
    context_->ClearDepthStencilView(dsv_.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

    // ★ステートを適用
    context_->RSSetState(rasterizerState_.Get());
    context_->OMSetDepthStencilState(depthStencilState_.Get(), 0);

    // パイプライン設定
    context_->IASetInputLayout(inputLayout_.Get());
    context_->VSSetShader(vs_.Get(), nullptr, 0);
    context_->VSSetConstantBuffers(0, 1, constantBuffer_.GetAddressOf());
    context_->PSSetShader(ps_.Get(), nullptr, 0);
}

void Renderer::SetTransform(const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj) {
	//定数バッファにワールド、ビュー、プロジェクション行列を転送する
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(context_->Map(constantBuffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        ConstantBuffer* cb = static_cast<ConstantBuffer*>(mapped.pData);
        cb->world_ = world;
        cb->view_ = view;
        cb->proj_ = proj;
        context_->Unmap(constantBuffer_.Get(), 0);
    }
}

void Renderer::EndFrame() {
	//スワップチェーンのバッファを表示する
    swapChain_->Present(1, 0);
}