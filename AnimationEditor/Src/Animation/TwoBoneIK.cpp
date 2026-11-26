#include "TwoBoneIK.h"
#include<algorithm>
#include<cmath>

using namespace DirectX;

namespace
{
	constexpr float kEpsilon = 1e-5f;
	constexpr float kPoleDistanceRatio = 0.5f;	//ポールの初期位置を、中間の関節からチェーンの長さの何倍離すか

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
		const float dot = std::clamp(XMVectorGetX(XMVector3Dot(f, t)), -1.0f, 1.0f);
		if (Length(axis) < kEpsilon)
		{
			if (dot > 0.0f) return XMQuaternionIdentity();	//ほぼ同じ向き
			//正反対の向きなら、垂直な軸で半回転
			XMVECTOR perp = XMVector3Cross(f, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
			if (Length(perp) < kEpsilon) perp = XMVector3Cross(f, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
			return XMQuaternionRotationNormal(XMVector3Normalize(perp), XM_PI);
		}
		return XMQuaternionRotationNormal(XMVector3Normalize(axis), std::acos(dot));
	}

	//vから、軸axis(正規化済み)の成分を除いたもの(axisに垂直な成分)
	XMVECTOR RemoveComponent(FXMVECTOR v, FXMVECTOR axis)
	{
		return v - axis * XMVector3Dot(v, axis);
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
	Pose& inOutPose, int& outRootBone, int& outMidBone, const XMFLOAT3* polePosition)
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
	const float lenAC = Length(c - a);
	if (lenAB < kEpsilon || lenBC < kEpsilon || lenAC < kEpsilon) return false;

	//根元から目標までの距離(届かない・近すぎるときは、届く範囲に収める)
	const float rawDist = Length(t - a);
	if (rawDist < kEpsilon) return false;
	const XMVECTOR dirAT = (t - a) / rawDist;
	const float dist = std::clamp(rawDist, std::abs(lenAB - lenBC) + kEpsilon, lenAB + lenBC - kEpsilon);

	//---------- チェーン全体を、根元→目標の向きへ向ける ----------
	//今の曲がる向き(中間の関節が、根元→先端の線からどちら側にずれているか)
	const XMVECTOR dirAC = (c - a) / lenAC;
	const XMVECTOR oldBend = RemoveComponent(b - a, dirAC);
	const bool hasOldBend = Length(oldBend) > lenAB * 1e-3f;	//まっすぐ伸びていると向きが決まらない

	//根元→先端 を 根元→目標 に向ける回転
	const XMVECTOR aimRotation = FromToRotation(c - a, t - a);
	//向けたあとの、今の曲がる向き
	XMVECTOR aimedBend = hasOldBend ? RemoveComponent(XMVector3Rotate(oldBend, aimRotation), dirAT) : XMVectorZero();
	const bool hasAimedBend = Length(aimedBend) > kEpsilon;
	if (hasAimedBend) aimedBend = XMVector3Normalize(aimedBend);

	//---------- 曲がる向き(ひじ・ひざの向き) ----------
	//ポールがあれば、根元→目標の線から見たポールの方向へ曲げる
	//無ければ今の曲がる向きのまま
	XMVECTOR bendDir = XMVectorZero();
	if (polePosition != nullptr)
	{
		bendDir = RemoveComponent(XMLoadFloat3(polePosition) - a, dirAT);
	}
	if (Length(bendDir) < kEpsilon)
	{
		bendDir = hasAimedBend ? aimedBend : AnyPerpendicular(dirAT);
	}
	bendDir = XMVector3Normalize(bendDir);

	//今の曲がる向きを、根元→目標の線を軸に回して bendDir に合わせる
	//(ひざ・ひじの曲がる軸ごと回すので、関節がねじれて見えない)
	XMVECTOR swingRotation = XMQuaternionIdentity();
	if (hasAimedBend)
	{
		const float sinSwing = XMVectorGetX(XMVector3Dot(XMVector3Cross(aimedBend, bendDir), dirAT));
		const float cosSwing = XMVectorGetX(XMVector3Dot(aimedBend, bendDir));
		swingRotation = XMQuaternionRotationNormal(dirAT, std::atan2(sinSwing, cosSwing));
	}
	//ここまでの根元の回転(向ける → 軸まわりに回す)
	const XMVECTOR planeRotation = XMQuaternionMultiply(aimRotation, swingRotation);
	const XMVECTOR b1 = a + XMVector3Rotate(b - a, planeRotation);	//回したあとの中間の関節

	//---------- 余弦定理で、中間の関節の行き先を求める ----------
	//根元の角度(根元→目標 と 根元→中間 のなす角)
	const float cosA = std::clamp((lenAB * lenAB + dist * dist - lenBC * lenBC) / (2.0f * lenAB * dist), -1.0f, 1.0f);
	const float sinA = std::sqrt(1.0f - cosA * cosA);
	const XMVECTOR newB = a + (dirAT * cosA + bendDir * sinA) * lenAB;	//中間の関節の行き先
	const XMVECTOR reachT = a + dirAT * dist;							//届く範囲に収めた目標

	//---------- 回転を求める ----------
	//根元を(同じ平面の中で)さらに回して、中間の関節を行き先へ
	const XMVECTOR rootDelta = XMQuaternionMultiply(planeRotation, FromToRotation(b1 - a, newB - a));
	//根元を回したあとの先端の位置
	const XMVECTOR c1 = a + XMVector3Rotate(c - a, rootDelta);
	//中間を回して、先端を目標へ
	const XMVECTOR midDelta = FromToRotation(c1 - newB, reachT - newB);

	//ワールドの回転(DirectXMathは「元の回転のあとに足す回転」を右に掛ける)
	//子は親の回転についていくので、中間には根元の回転も掛かる
	const XMVECTOR parentWorld = (rootParent >= 0) ? GetRotation(worldMatrices[rootParent]) : XMQuaternionIdentity();
	const XMVECTOR rootWorld = XMQuaternionMultiply(GetRotation(worldMatrices[root]), rootDelta);
	const XMVECTOR midWorld = XMQuaternionMultiply(XMQuaternionMultiply(GetRotation(worldMatrices[mid]), rootDelta), midDelta);

	//ローカルの回転に戻す(ワールド = ローカル × 親のワールド なので、ローカル = ワールド × 親のワールドの逆)
	XMStoreFloat4(&inOutPose[root].rotation_,
		XMQuaternionNormalize(XMQuaternionMultiply(rootWorld, XMQuaternionInverse(parentWorld))));
	XMStoreFloat4(&inOutPose[mid].rotation_,
		XMQuaternionNormalize(XMQuaternionMultiply(midWorld, XMQuaternionInverse(rootWorld))));

	//先端は、ワールドの向きが目標の向きになるようにする
	//(中間が回っても、足首や手首の向きがつられて変わらないように)
	const XMVECTOR endWorld = XMQuaternionNormalize(XMLoadFloat4(&targetRotation));
	XMStoreFloat4(&inOutPose[endBone].rotation_,
		XMQuaternionNormalize(XMQuaternionMultiply(endWorld, XMQuaternionInverse(midWorld))));

	outRootBone = root;
	outMidBone = mid;
	return true;
}

bool TwoBoneIK::ComputeDefaultPole(const Skeleton& skeleton, const std::vector<XMFLOAT4X4>& worldMatrices,
	int endBone, XMFLOAT3& outPolePosition)
{
	if (endBone < 0 || endBone >= static_cast<int>(worldMatrices.size())) return false;

	int root = -1;
	int mid = -1;
	if (!FindChain(skeleton, endBone, root, mid)) return false;

	const XMVECTOR a = GetPosition(worldMatrices[root]);
	const XMVECTOR b = GetPosition(worldMatrices[mid]);
	const XMVECTOR c = GetPosition(worldMatrices[endBone]);
	const float lenAC = Length(c - a);
	if (lenAC < kEpsilon) return false;

	//今の曲がる向き(まっすぐ伸びているときは適当な垂直方向)
	const XMVECTOR dirAC = (c - a) / lenAC;
	XMVECTOR bend = RemoveComponent(b - a, dirAC);
	bend = (Length(bend) < kEpsilon) ? AnyPerpendicular(dirAC) : XMVector3Normalize(bend);

	//中間の関節から、チェーンの長さの半分くらい離す
	const float distance = (Length(b - a) + Length(c - b)) * kPoleDistanceRatio;
	XMStoreFloat3(&outPolePosition, b + bend * distance);
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