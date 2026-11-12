#pragma once
#include<DxLib.h>

class Camera
{
public:
	Camera(void);
	~Camera(void) = default;
	
	/// <summary>
	/// XV
	/// </summary>
	/// <param name="UsingUI">UI‚ğ‘€ì’†</param>
	void Update(bool UsingUI = false);

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