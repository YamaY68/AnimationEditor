#pragma once
#include<vector>
#include"../Common/Transform.h"

/// <summary>
/// 全ボーン分のローカル姿勢(ある1フレームの姿勢)
/// </summary>
struct Pose
{
	std::vector<Transform> localTransforms_;

	//ボーン数に合わせてサイズを変える
	void Resize(size_t boneCount) { localTransforms_.resize(boneCount); }
	//ボーン数
	size_t GetBoneCount(void)const { return localTransforms_.size(); }

	//指定ボーンの姿勢
	Transform& operator[](size_t index) { return localTransforms_[index]; }
	const Transform& operator[](size_t index)const { return localTransforms_[index]; }
};