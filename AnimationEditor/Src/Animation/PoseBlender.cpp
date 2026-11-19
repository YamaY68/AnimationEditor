#include "PoseBlender.h"
#include<algorithm>

void PoseBlender::Blend(const Pose& poseA, const Pose& poseB, float weight, Pose& outPose)
{
	weight = std::clamp(weight, 0.0f, 1.0f);

	//ƒ{[ƒ“”‚ª­‚È‚¢•û‚É‡‚í‚¹‚é
	const size_t count = std::min(poseA.GetBoneCount(), poseB.GetBoneCount());
	if (outPose.GetBoneCount() < count)
	{
		outPose.Resize(count);
	}

	for (size_t i = 0; i < count; ++i)
	{
		outPose[i] = Transform::Interpolate(poseA[i], poseB[i], weight);
	}
}