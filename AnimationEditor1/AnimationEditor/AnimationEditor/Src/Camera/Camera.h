#pragma once
#include<DxLib.h>

class Camera
{
public:
	Camera(void);
	~Camera(void) = default;

	/// <summary>
	/// 更新
	/// </summary>
	/// <param name="allowInput">UI操作中などでない場合true</param>
	void Update(bool allowInput = true);
	//DxLib描画パイプラインにカメラ行列を適用
	void ApplyToDxLib(void)const;

	void SetTarget(const VECTOR& target) { target_ = target; }
	VECTOR GetTarget(void)const { return target_; }

	void SetDistance(float distance) { distance_ = distance; }
	float GetDistance(void)const { return distance_; }

private:
	VECTOR target_;
	float distance_;
	float yaw_;
	float pitch_;

	int prevMouseX_;
	int prevMouseY_;
};

