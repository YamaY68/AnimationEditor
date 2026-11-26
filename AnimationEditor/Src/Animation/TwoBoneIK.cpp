#include "TwoBoneIK.h"
#include<algorithm>
#include<cmath>

using namespace DirectX;

namespace
{
	constexpr float kEpsilon = 1e-5f;

	//ワールド行列の平行移動成分(＝ボーンの位置)
	XMVECTOR GetPosition(const XMFLOAT4X4& m)
	{
		return XMVectorSet(m._41, m._42, m._43, 0.0f);
	}

	//ワールド行列から回転だけを取り出す
	XMVECTOR GetRotation(const XMFLOAT4X4& m)
	{
		XMVECTOR s, r, t;
		if (!XMMatrixDecompose(&s, &r, &t, XMLoadFloat4x4(&m))) return XMQuaternionIdentity();
		return XMQuaternionNormalize(r);
	}

	float Length(FXMVECTOR v)
	{
		return XMVectorGetX(XMVector3Length(v));
	}

	//向きfromを、向きtoに合わせる最小の回転
	XMVECTOR FromToRotation(FXMVECTOR from, FXMVECTOR to)
	{
		const XMVECTOR f = XMVector3Normalize(from);
		const XMVECTOR t = XMVector3Normalize(to);
		const XMVECTOR axis = XMVector3Cross(f, t);
		if (Length(axis) < kEpsilon) return XMQuaternionIdentity();	//ほぼ同じ向き

		const float dot = std::clamp(XMVectorGetX(XMVector3Dot(f, t)), -1.0f, 1.0f);
		return XMQuaternionRotationNormal(XMVector3Normalize(axis), std::acos(dot));
	}

	//dirに垂直な向きを1つ求める(関節がまっすぐ伸びていて、曲がる向きが決まらないときに使う)
	XMVECTOR AnyPerpendicular(FXMVECTOR dir)
	{
		XMVECTOR perp = XMVector3Cross(dir, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
		if (Length(perp) < kEpsilon)
		{
			perp = XMVector3Cross(dir, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
		}
		return XMVector3Normalize(perp);
	}
	//子のボーンの数
	int CountChildren(const Skeleton& skeleton, int boneIndex)
	{
		int count = 0;
		for (size_t i = 0; i < skeleton.GetBoneCount(); ++i)
		{
			if (skeleton.GetBone(i).parentIndex_ == boneIndex) ++count;
		}
		return count;
	}
}

bool TwoBoneIK::Solve(const Skeleton& skeleton, const std::vector<XMFLOAT4X4>& worldMatrices,
	int endBone, const XMFLOAT3& targetPosition, const XMFLOAT4& targetRotation,
	Pose& inOutPose, int& outRootBone, int& outMidBone) 
{
	//---------- チェーンを作る(先端 → 親 → 親の親) ----------
	const int boneCount = static_cast<int>(std::min(worldMatrices.size(), inOutPose.GetBoneCount()));
	if (endBone < 0 || endBone >= boneCount) return false;

	int root = -1;
	int mid = -1;
	if (!FindChain(skeleton, endBone, root, mid)) return false;
	const int rootParent = skeleton.GetBone(root).parentIndex_;

	//---------- 今の関節の位置とボーンの長さ ----------
	const XMVECTOR a = GetPosition(worldMatrices[root]);	//根元
	const XMVECTOR b = GetPosition(worldMatrices[mid]);		//中間
	const XMVECTOR c = GetPosition(worldMatrices[endBone]);	//先端
	const XMVECTOR t = XMLoadFloat3(&targetPosition);

	const float lenAB = Length(b - a);
	const float lenBC = Length(c - b);
	if (lenAB < kEpsilon || lenBC < kEpsilon) return false;

	//根元から目標までの距離(届かない・近すぎるときは、届く範囲に収める)
	const float rawDist = Length(t - a);
	if (rawDist < kEpsilon) return false;
	const XMVECTOR dirAT = (t - a) / rawDist;
	const float dist = std::clamp(rawDist, std::abs(lenAB - lenBC) + kEpsilon, lenAB + lenBC - kEpsilon);

	//---------- 曲がる向き ----------
	//今の中間の関節が、根元→目標の線からどちら側にずれているか(ひじ・ひざの向き)
	XMVECTOR bendDir = (b - a) - dirAT * XMVector3Dot(b - a, dirAT);
	bendDir = (Length(bendDir) < kEpsilon) ? AnyPerpendicular(dirAT) : XMVector3Normalize(bendDir);

	//---------- 余弦定理で、中間の関節の行き先を求める ----------
	//根元の角度(根元→目標 と 根元→中間 のなす角)
	const float cosA = std::clamp((lenAB * lenAB + dist * dist - lenBC * lenBC) / (2.0f * lenAB * dist), -1.0f, 1.0f);
	const float sinA = std::sqrt(1.0f - cosA * cosA);
	const XMVECTOR newB = a + (dirAT * cosA + bendDir * sinA) * lenAB;	//中間の関節の行き先
	const XMVECTOR reachT = a + dirAT * dist;							//届く範囲に収めた目標

	//---------- 回転を求める ----------
	//根元を回して、中間の関節を行き先へ
	const XMVECTOR rootDelta = FromToRotation(b - a, newB - a);
	//根元を回したあとの先端の位置
	const XMVECTOR c1 = a + XMVector3Rotate(c - a, rootDelta);
	//中間を回して、先端を目標へ
	const XMVECTOR midDelta = FromToRotation(c1 - newB, reachT - newB);

	//ワールドの回転(DirectXMathは「元の回転のあとに足す回転」を右に掛ける)
	//子は親の回転についていくので、中間には根元の回転も掛かる
	const XMVECTOR parentWorld = (rootParent >= 0) ? GetRotation(worldMatrices[rootParent]) : XMQuaternionIdentity();
	const XMVECTOR rootWorld = XMQuaternionMultiply(GetRotation(worldMatrices[root]), rootDelta);
	const XMVECTOR midWorld = XMQuaternionMultiply(XMQuaternionMultiply(GetRotation(worldMatrices[mid]), rootDelta), midDelta);

	XMStoreFloat4(&inOutPose[mid].rotation_,
		XMQuaternionNormalize(XMQuaternionMultiply(midWorld, XMQuaternionInverse(rootWorld))));

	//先端は、ワールドの向きが目標の向きになるようにする
	//(中間が回っても、足首や手首の向きがつられて変わらないように)
//根元のローカル回転(ワールド = ローカル × 親のワールド なので、ローカル = ワールド × 親のワールドの逆)
	XMStoreFloat4(&inOutPose[root].rotation_,
		XMQuaternionNormalize(XMQuaternionMultiply(rootWorld, XMQuaternionInverse(parentWorld))));
	outRootBone = root;
	outMidBone = mid;
	return true;
}

bool TwoBoneIK::FindChain(const Skeleton& skeleton, int endBone, int& outRootBone, int& outMidBone)
{
	if (endBone < 0 || endBone >= static_cast<int>(skeleton.GetBoneCount())) return false;

	const int mid = skeleton.GetBone(endBone).parentIndex_;
	if (mid < 0) return false;
	const int root = skeleton.GetBone(mid).parentIndex_;
	if (root < 0) return false;

	//分かれ道のボーン(子が複数)を回すと、チェーン以外の部位まで動くので使わない
	if (CountChildren(skeleton, mid) != 1) return false;
	if (CountChildren(skeleton, root) != 1) return false;

	outRootBone = root;
	outMidBone = mid;
	return true;
}