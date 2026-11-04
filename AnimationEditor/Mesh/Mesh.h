#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>
#include<string>
/// <summary>
/// 3Dモデル用頂点データ構造体（座標・法線・UV）
/// </summary>
struct Vertex {
	DirectX::XMFLOAT3 pos;    // 12 bytes [0..11]
	DirectX::XMFLOAT4 col_;   // 16 bytes [12..27]
	DirectX::XMFLOAT3 normal; // 12 bytes [28..39]
	DirectX::XMFLOAT2 uv;     // 8 bytes  [40..47]
};

class Mesh
{
public:
	Mesh() = default;
	~Mesh() = default;
	/// <summary>
	/// テスト用キューブ
	/// </summary>
	/// <param name="device"></param>
	/// <returns></returns>
	bool CreateCube(ID3D11Device* device);

	/// <summary>
	/// メッシュの描画
	/// </summary>
	/// <param name="context"></param>
	void Draw(ID3D11DeviceContext* context);

	/// <summary>
	/// 登録されてるインデックスの総数を取得
	/// </summary>
	/// <param name=""></param>
	/// <returns></returns>
	UINT GetIndexCount(void)const { return indexCount_; }

	/// <summary>
	/// FBXファイルからメッシュデータを読み込みGPUバッファを生成
	/// </summary>
	/// <param name="device"></param>
	/// <param name="filePath"></param>
	/// <returns></returns>
	bool LoadFBX(ID3D11Device* device, const std::string& filePath);

private:
	Microsoft::WRL::ComPtr<ID3D11Buffer>vertexBuffer_;	//GPU頂点バッファ
	Microsoft::WRL::ComPtr<ID3D11Buffer>indexBuffer_;	//GPUインデっクスバッファ
	UINT indexCount_ = 0;	//インデックスの総数

};

