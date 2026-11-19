#pragma once
#include"Pose.h"

/// <summary>
/// 2‚Â‚Ìp¨‚ğ¬‚º‚éˆ—
/// </summary>
namespace PoseBlender
{
	/// <summary>
	/// 2‚Â‚Ìp¨‚ğweight‚ÌŠ„‡‚Å¬‚º‚é
	/// </summary>
	/// <param name="poseA">weight=0‚Ì‚Æ‚«‚Ìp¨</param>
	/// <param name="poseB">weight=1‚Ì‚Æ‚«‚Ìp¨</param>
	/// <param name="weight">¬‚º‚éŠ„‡(0`1)</param>
	/// <param name="outPose">Œ‹‰Ê(ƒ{[ƒ“”‚ª‘«‚è‚È‚¯‚ê‚ÎL‚°‚é)</param>
	void Blend(const Pose& poseA, const Pose& poseB, float weight, Pose& outPose);
}