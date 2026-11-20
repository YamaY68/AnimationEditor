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
	/// <param name="UsingUI">UIを操作中</param>
	void Update(bool UsingUI = false);

	void ApplyToDxLib(void)const;

	void SetTarget(const VECTOR& target) { target_ = target; }
	VECTOR GetTarget(void)const { return target_; }

	void SetDistance(float distance) { distance_ = distance; }
	float GetDistance(void)const { return distance_; }
	/// <summary>
/// 指定した球がちょうど画面に収まるように、注視点・距離・描画範囲を合わせる
/// (モデルの大きさはモデルごとにバラバラなので、読み込んだときに合わせる)
/// </summary>
/// <param name="center">球の中心</param>
/// <param name="radius">球の半径</param>
	void Frame(const VECTOR& center, float radius);
private:
	VECTOR target_;
	float distance_;
	float yaw_;
	float pitch_;
	float minDistance_;	//注視点に近づける限界
	float maxDistance_;	//注視点から離れられる限界
	float nearClip_;	//これより手前は描かない
	float farClip_;		//これより奥は描かない
	int prevMouseX_;
	int prevMouseY_;

};