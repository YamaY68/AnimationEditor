#include "PoseBlender.h"
#include<cmath>

void PoseBlender::Blend(const Pose& poseA, const Pose& poseB, float weight, Pose& outPose)
{
	weight = std::clamp(weight, 0.0f, 1.0f);

	//２つのボーンのうちボーン数が少ないほうに合わせてループ上限を設定
	size_t count = std::min(poseA.GetBoneCoount(), poseB.GetBoneCoount());

	//出力先バッファの要素が不足していればリサイズ
	if (outPose.GetBoneCoount() < count)
	{
		outPose.Resize(count);
	}

	for (size_t i = 0; i < count; ++i)
	{
		outPose[i] = Transform::Interpolate(poseA[i], poseB[i], weight);
	}
}

void PoseBlender::BlendMasked(const Pose& poseA, const Pose& poseB, const std::vector<float>& boneMask, Pose& outPose)
{
	size_t count = std::min(poseA.GetBoneCoount(), poseB.GetBoneCoount());
	count = std::min(count, boneMask.size());

	if (outPose.GetBoneCoount() < count)
	{
		outPose.Resize(count);
	}

	for (size_t i = 0; i < count; ++i)
	{
		float w = std::clamp(boneMask[i], 0.0f, 1.0f);
		outPose[i] = Transform::Interpolate(poseA[i], poseB[i], w);
	}
}
