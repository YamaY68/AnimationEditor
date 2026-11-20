#include "Camera.h"
#include<cmath>
#include<algorithm>
namespace
{
	constexpr float kDefaultMinDistance = 1.0f;		//近づける限界(初期値)
	constexpr float kDefaultMaxDistance = 1000.0f;	//離れられる限界(初期値)
	constexpr float kDefaultNearClip = 1.0f;		//描画範囲の手前(初期値)
	constexpr float kDefaultFarClip = 3000.0f;		//描画範囲の奥(初期値)

	//Frameで使う割合(すべて半径に対する倍率)
	constexpr float kFrameDistanceRate = 2.5f;		//カメラの距離
	constexpr float kFrameMinDistanceRate = 0.01f;	//近づける限界
	constexpr float kFrameMaxDistanceRate = 20.0f;	//離れられる限界
	constexpr float kFrameNearClipRate = 0.01f;		//描画範囲の手前
	constexpr float kFrameFarClipRate = 50.0f;		//描画範囲の奥
}
Camera::Camera(void)
	:
	target_(VGet(0.0f, 10.0f, 0.0f)),
	distance_(50.0f),
	yaw_(0.0f),
	pitch_(0.0f),
	prevMouseX_(0),
	prevMouseY_(0),
	minDistance_(kDefaultMinDistance),
	maxDistance_(kDefaultMaxDistance),
	nearClip_(kDefaultNearClip),
	farClip_(kDefaultFarClip)
{
}

void Camera::Update(bool UsingUI)
{
	int mouseX, mouseY;
	GetMousePoint(&mouseX, &mouseY);
	int wheel = GetMouseWheelRotVol();

	if (UsingUI)
	{
		int deltaX = mouseX - prevMouseX_;
		int deltaY = mouseY - prevMouseY_;

		//ホイール回転によるズームドリー
		if (wheel != 0)
		{
			float zoomPow = 0.08f;
			distance_ -= wheel * (distance_ * zoomPow);
			distance_ = std::clamp(distance_, minDistance_, maxDistance_);
		}

		int mouseInput = GetMouseInput();

		//右クリックによる注視点回転
		if ((mouseInput & MOUSE_INPUT_RIGHT) != 0)
		{
			const float yawSpeed = 0.003f;
			const float pitchSpeed = 0.004f;

			yaw_ += deltaX * yawSpeed;
			pitch_ -= deltaY * pitchSpeed;

			//真下、真上付近の特異点回避
			const float piptchLimit = 89.5f * (DX_PI_F / 180);
			pitch_ = std::clamp(pitch_, -piptchLimit, piptchLimit);
		}

		//ホイール押し込み、平行移動
		if ((mouseInput & MOUSE_INPUT_MIDDLE) != 0)
		{
			float rightX = std::cos(yaw_);
			float rightY = std::sin(yaw_);

			float panSpeed = distance_ * 0.001f;
			target_.x += rightX * deltaX * panSpeed;
			target_.z -= rightY * deltaX * panSpeed;
			target_.y += deltaY * panSpeed;
		}
	}
	prevMouseX_ = mouseX;
	prevMouseY_ = mouseY;
}

void Camera::ApplyToDxLib(void) const
{
	VECTOR eye;
	eye.x = target_.x + distance_ * std::cos(pitch_) * std::sin(yaw_);
	eye.y = target_.y + distance_ * std::sin(pitch_);
	eye.z = target_.z + distance_ * std::cos(pitch_) * std::cos(yaw_);

	VECTOR up = VGet(0.0f, 1.0f, 0.0f);
	SetCameraPositionAndTargetAndUpVec(eye, target_, up);
	SetCameraNearFar(nearClip_, farClip_);
}

void Camera::Frame(const VECTOR& center, float radius)
{
	if (radius <= 0.0f) return;

	target_ = center;
	distance_ = radius * kFrameDistanceRate;
	//近づける・離れられる範囲と描画範囲も、モデルの大きさに合わせる
	//(固定のままだと、大きいモデルは遠くが切れ、小さいモデルは近づけない)
	minDistance_ = radius * kFrameMinDistanceRate;
	maxDistance_ = radius * kFrameMaxDistanceRate;
	nearClip_ = radius * kFrameNearClipRate;
	farClip_ = radius * kFrameFarClipRate;
}