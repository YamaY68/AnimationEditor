#include "IKPinSet.h"
#include<algorithm>
#include"../Animation/TwoBoneIK.h"

using namespace DirectX;

bool IKPinSet::Has(int endBone) const
{
	return std::any_of(pins_.begin(), pins_.end(), [endBone](const IKPin& p)
		{
			return p.endBone == endBone;
		});
}

bool IKPinSet::Toggle(int endBone, const std::vector<XMFLOAT4X4>& worldMatrices)
{
	//すでにピン留めされていれば外す
	auto it = std::find_if(pins_.begin(), pins_.end(), [endBone](const IKPin& p)
		{
			return p.endBone == endBone;
		});
	if (it != pins_.end())
	{
		pins_.erase(it);
		return false;
	}

	if (endBone < 0 || endBone >= static_cast<int>(worldMatrices.size())) return false;

	//今の先端の位置と向きで固定する
	const XMFLOAT4X4& m = worldMatrices[endBone];
	IKPin pin;
	pin.endBone = endBone;
	pin.position = { m._41, m._42, m._43 };

	XMVECTOR s, r, t;
	if (XMMatrixDecompose(&s, &r, &t, XMLoadFloat4x4(&m)))
	{
		XMStoreFloat4(&pin.rotation, XMQuaternionNormalize(r));
	}
	pins_.push_back(pin);
	return true;
}

void IKPinSet::Set(int endBone, const XMFLOAT3& position, const XMFLOAT4& rotation)
{
	for (IKPin& pin : pins_)
	{
		if (pin.endBone == endBone)
		{
			pin.position = position;
			pin.rotation = rotation;
			return;
		}
	}
}

void IKPinSet::SetPole(int endBone, const XMFLOAT3& polePosition)
{
	for (IKPin& pin : pins_)
	{
		if (pin.endBone == endBone)
		{
			pin.hasPole = true;
			pin.polePosition = polePosition;
			return;
		}
	}
}

const IKPin* IKPinSet::Find(int endBone) const
{
	for (const IKPin& pin : pins_)
	{
		if (pin.endBone == endBone) return &pin;
	}
	return nullptr;
}

std::vector<int> IKPinSet::Apply(const Skeleton& skeleton, Pose& inOutPose) const
{
	std::vector<int> editedBones;
	std::vector<XMFLOAT4X4> worldMatrices;

	for (const IKPin& pin : pins_)
	{
		//前のピンで姿勢が変わっているかもしれないので、ピンごとにワールド行列を求め直す
		skeleton.ComputeWorldMatrices(inOutPose, worldMatrices);

		int rootBone = -1;
		int midBone = -1;
		if (TwoBoneIK::Solve(skeleton, worldMatrices, pin.endBone,
			pin.position, pin.rotation, inOutPose, rootBone, midBone,
			pin.hasPole ? &pin.polePosition : nullptr))
		{
			editedBones.push_back(rootBone);
			editedBones.push_back(midBone);
			editedBones.push_back(pin.endBone);
		}
	}
	return editedBones;
}