#pragma once
#include <DirectXMath.h>

class Camera
{
public:
	/// <summary>
	/// コンストラクタ
	/// </summary>
	/// <param name="fov">視野角（ラジアン単位: DirectX::XMConvertToRadians 等で変換）</param>
	/// <param name="aspect">アスペクト比</param>
	/// <param name="nearZ">前方クリップ距離</param>
	/// <param name="farZ">後方クリップ距離</param>
	Camera(float fov, float aspect, float nearZ, float farZ)
		: eye_(DirectX::XMVectorSet(0.0f, 2.0f, -5.0f, 1.0f)),
		target_(DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f)),
		up_(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f))
	{
		proj_ = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);
		UpdateView();
	}

	void SetPosition(const DirectX::XMFLOAT3& pos)
	{
		eye_ = DirectX::XMLoadFloat3(&pos);
		UpdateView();
	}

	void SetTarget(const DirectX::XMFLOAT3& target)
	{
		target_ = DirectX::XMLoadFloat3(&target);
		UpdateView();
	}

	/// <summary>
	/// 画面サイズ変更時などにプロジェクション行列を再構築
	/// </summary>
	void SetProjection(float fov, float aspect, float nearZ, float farZ) {
		proj_ = DirectX::XMMatrixPerspectiveFovLH(fov, aspect, nearZ, farZ);
	}

	DirectX::XMMATRIX GetViewMatrix() const { return view_; }
	DirectX::XMMATRIX GetProjMatrix() const { return proj_; }
	DirectX::XMVECTOR GetEye() const { return eye_; }
	DirectX::XMVECTOR GetTarget() const { return target_; }

private:
	void UpdateView(void) {
		// メンバ変数の eye_, target_, up_ からビュー行列を構築
		view_ = DirectX::XMMatrixLookAtLH(eye_, target_, up_);
	}

	DirectX::XMVECTOR eye_;    // カメラの視点位置
	DirectX::XMVECTOR target_; // 注視点位置
	DirectX::XMVECTOR up_;     // カメラの上方向ベクトル
	DirectX::XMMATRIX view_;   // ビュー行列
	DirectX::XMMATRIX proj_;   // プロジェクション行列
};