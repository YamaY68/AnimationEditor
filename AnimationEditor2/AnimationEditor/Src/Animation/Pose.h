#pragma once
#include<vector>
#include"../Common/Transform.h"

//全ボーン１フレーム分のローカル姿勢
struct Pose
{
	std::vector<Transform>localTransforms_;
	//ボーン数に合わせてリサいず
	void Resize(size_t boneCount) { localTransforms_.resize(boneCount); }
	//保持しているボーンの総数を取得
	size_t GetBoneCoount(void)const { return localTransforms_.size(); }

	//指定インデっクスのTransformへの参照を取得
	Transform& operator[](size_t index) { return localTransforms_[index]; }
	const Transform& operator[](size_t index)const { return localTransforms_[index]; }
};