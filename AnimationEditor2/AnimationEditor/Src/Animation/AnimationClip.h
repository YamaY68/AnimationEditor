#pragma once
#include<string>
#include<vector>
#include<cmath>
#include"AnimationTrack.h"
#include"Skeleton.h"
#include"Pose.h"

class AnimationClip
{
public:
	AnimationClip(void) = default;
	AnimationClip(const std::string& name, float duration)
		:name_(name), duration_(duration) {}

	BoneTrack* GetOrCreateTrack(int boneIndex);
	void Sample(float time, const Skeleton& skeleton, Pose& outPose)const;


	std::string name_;
	float duration_ = 0.0f;
	std::vector<BoneTrack>boneTracks_;
private:
	
};

