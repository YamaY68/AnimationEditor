#pragma once
#include<vector>
#include"../Common/Transform.h"

struct Pose
{
	std::vector<Transform>localTransforms_;
	void Resize(size_t boneCount)
	{
		localTransforms_.resize(boneCount);
	}

	size_t GetBoneCount(void)const
	{
		return localTransforms_.size();
	}
};