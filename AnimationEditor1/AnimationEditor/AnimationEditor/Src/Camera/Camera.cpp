#include "Camera.h"
#include<cmath>
#include<algorithm>


Camera::Camera(void)
	:
	target_(VGet(0.0f,10.0f,0.0f)),
	distance_(50.0f),
	yaw_(0.0f),
	pitch_(0.0f),
	prevMouseX_(0),
	prevMouseY_(0)
{
}

void Camera::Update(bool allowInput) {
    int mouseX, mouseY;
    GetMousePoint(&mouseX, &mouseY);
    int wheel = GetMouseWheelRotVol();

    if (allowInput) {
        int deltaX = mouseX - prevMouseX_;
        int deltaY = mouseY - prevMouseY_;

        //ホイール回転によるズーム/ドリー
        if (wheel != 0) {
            //ホイール1ノッチあたりのズーム量を調整
            float zoomFactor = 0.08f;
            distance_ -= wheel * (distance_ * zoomFactor);
            // 距離の最小値・最大値制限
            distance_ = std::clamp(distance_, 1.0f, 1000.0f);
        }

        int mouseInput = GetMouseInput();

        // 右ボタンドラッグ オービット回転
        if ((mouseInput & MOUSE_INPUT_RIGHT) != 0) {
            const float yawSpeed = 0.003f; // 水平の回転速度
            const float pitchSpeed = 0.004f; // 垂直の回転速度

            yaw_ += deltaX * yawSpeed;
            pitch_ -= deltaY * pitchSpeed; // 下に引くと見下ろし、上に押すと見上げる向きに調整

            // 真上・真下付近の特異点回避（-89.5度 〜 +89.5度）
            const float pitchLimit = 89.5f * (DX_PI_F / 180.0f);
            pitch_ = std::clamp(pitch_, -pitchLimit, pitchLimit);
        }

        //ホイール押し込み パン移動
        if ((mouseInput & MOUSE_INPUT_MIDDLE) != 0) {
            float rightX = std::cos(yaw_);
            float rightZ = -std::sin(yaw_);

            //距離に応じたパン速度
            float panSpeed = distance_ * 0.001f;
            target_.x -= (-rightX * deltaX) * panSpeed;
            target_.z -= (-rightZ * deltaX) * panSpeed;
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
	SetCameraPositionAndTarget_UpVecY(eye, target_);
}
