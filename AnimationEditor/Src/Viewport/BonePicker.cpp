#include "BonePicker.h"
#include<DxLib.h>
#include<algorithm>

namespace
{
	//ボーンの位置をスクリーン座標に変換する
	//カメラの後ろや描画範囲の外ならfalse
	bool ToScreen(const DirectX::XMFLOAT4X4& m, VECTOR& outScreen)
	{
		outScreen = ConvWorldPosToScreenPos(VGet(m._41, m._42, m._43));
		return outScreen.z >= 0.0f && outScreen.z <= 1.0f;
	}

	//点pと線分abの距離の2乗(画面上の2D)
	float DistSqToSegment(float px, float py, float ax, float ay, float bx, float by)
	{
		const float abx = bx - ax;
		const float aby = by - ay;
		const float lenSq = abx * abx + aby * aby;

		//線分上で点pに一番近い位置(0:a ～ 1:b)
		float t = 0.0f;
		if (lenSq > 0.0f)
		{
			t = std::clamp(((px - ax) * abx + (py - ay) * aby) / lenSq, 0.0f, 1.0f);
		}
		const float dx = px - (ax + abx * t);
		const float dy = py - (ay + aby * t);
		return dx * dx + dy * dy;
	}
}

int BonePicker::Pick(const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& worldMatrices,
	float mouseX, float mouseY, float jointRadiusPx, float lineRadiusPx)
{
	const int count = static_cast<int>(min(skeleton.GetBoneCount(), worldMatrices.size()));

	//各ボーンのスクリーン座標を先に求めておく(関節とラインの両方で使う)
	std::vector<VECTOR> screens(count);
	std::vector<bool> visible(count);
	for (int i = 0; i < count; ++i)
	{
		visible[i] = ToScreen(worldMatrices[i], screens[i]);
	}

	//---------- 関節(優先) ----------
	int bestJoint = -1;
	float bestJointDistSq = jointRadiusPx * jointRadiusPx;
	for (int i = 0; i < count; ++i)
	{
		if (!visible[i]) continue;

		const float dx = screens[i].x - mouseX;
		const float dy = screens[i].y - mouseY;
		const float distSq = dx * dx + dy * dy;
		if (distSq < bestJointDistSq)
		{
			bestJointDistSq = distSq;
			bestJoint = i;
		}
	}
	if (bestJoint >= 0) return bestJoint;

	//---------- ライン(親→子) ----------
	int bestLine = -1;
	float bestLineDistSq = lineRadiusPx * lineRadiusPx;
	for (int i = 0; i < count; ++i)
	{
		const int parent = skeleton.GetBone(i).parentIndex_;
		if (parent < 0 || parent >= count) continue;
		if (!visible[i] || !visible[parent]) continue;

		const float distSq = DistSqToSegment(mouseX, mouseY,
			screens[parent].x, screens[parent].y, screens[i].x, screens[i].y);
		if (distSq < bestLineDistSq)
		{
			bestLineDistSq = distSq;
			bestLine = parent;	//ラインは根元(親側)のボーンとして選ぶ
		}
	}
	return bestLine;
}