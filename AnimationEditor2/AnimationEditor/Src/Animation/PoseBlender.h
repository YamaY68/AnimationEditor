#pragma once
#include<vector>
#include<algorithm>
#include"Pose.h"

class PoseBlender
{
public:
	PoseBlender(void) = default;

	/// <summary>
	/// ２つのポーズを指定されたブレンド率でブレンド
	/// </summary>
	/// <param name="poseA">始点</param>
	/// <param name="poseB">終点</param>
	/// <param name="weight">ブレンド率</param>
	/// <param name="outPose">out</param>
	static void Blend(const Pose& poseA, const Pose& poseB, float weight, Pose& outPose);
	
	/// <summary>
	/// ボーンごとの適応マスクを用いて部分ブレンド
	/// </summary>
	/// <param name="poseA">ベースとなるポーズ</param>
	/// <param name="poseB">重ね合わせるポーズ</param>
	/// <param name="boneMask">ボーンごとのブレンド率配列(下半身0.0f,上半身1.0f等)</param>
	/// <param name="outPose">out</param>
	static void BlendMasked(const Pose& poseA, const Pose& poseB, const std::vector<float>& boneMask, Pose& outPose);

};

