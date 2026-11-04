#pragma once
#include<Windows.h>
#include<d3d11.h>
#include<DirectXMath.h>
#include<wrl/client.h>

struct alignas(16)ConstantBuffer {
	DirectX::XMMATRIX world_;	//ワールド変換行列
	DirectX::XMMATRIX view_;	//ビュー変換行列
	DirectX::XMMATRIX proj_;	//プロジェクション変換行列
};

class Renderer
{
public:
	Renderer() = default;
	~Renderer() = default;
	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="hWnd">描画対象のウィンドウハンドル</param>
	/// <param name="width">描画領域の幅</param>
	/// <param name="height">描画領域の高さ</param>
	/// <returns></returns>
	bool Init(HWND hWnd, UINT width, UINT height);
	/// <summary>
	/// フレームの描画準備
	/// </summary>
	/// <param name="clearColor"></param>
	void BeginFrame(const float clearColor[4]);
	/// <summary>
	/// スワップチェインのバックバッファを画面にフリップ
	/// </summary>
	void EndFrame();
	/// <summary>
	/// ワールド、ビュー、プロジェクション行列を定数バッファに転送
	/// </summary>
	/// <param name="world"></param>
	/// <param name="view"></param>
	/// <param name="proj"></param>
	void SetTransform(const DirectX::XMMATRIX& world, const DirectX::XMMATRIX& view, const DirectX::XMMATRIX& proj);
	ID3D11Device* GetDevice() const { return device_.Get(); }
	ID3D11DeviceContext* GetContext() const { return context_.Get(); }
private:
	/// <summary>
	/// デバイスとスワップチェーン、レンダーターゲット、深度バッファを生成
	/// </summary>
	/// <param name="hWnd"></param>
	/// <param name="width"></param>
	/// <param name="height"></param>
	/// <returns></returns>
	bool CreateDeviceAndSwapChain(HWND hWnd, UINT width, UINT height);

	/// <summary>
	/// シェーダーのコンパイルと入力レイアウト、定数バッファを生成
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	bool CreatePipeline(void);

	Microsoft::WRL::ComPtr<ID3D11Device> device_;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context_;
	Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain_;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv_;	//レンダーターゲットビュー
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv_;	//深度ステンシルビュー
	D3D11_VIEWPORT viewport_{};	

	Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;	//頂点シェーダー
	Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;	//ピクセルシェーダー
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;	//入力レイアウト
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer_;	//定数バッファ

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState_;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState_; // 追加
};

