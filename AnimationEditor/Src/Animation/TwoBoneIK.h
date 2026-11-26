#pragma once
#include<vector>
#include<DirectXMath.h>
#include"Skeleton.h"
#include"Pose.h"

/// <summary>
/// 2ボーンIK
/// 根元(親の親)・中間(親)・先端の3つの関節で、先端が目標に届くように
/// 根元と中間の回転を求める(余弦定理で解く)
/// </summary>
namespace TwoBoneIK
{
	/// <summary>
	/// 先端のボーンが目標の位置に届くよう、根元と中間のボーンのローカル回転を書き換える
	/// 先端のボーンは、ワールドの向きが targetRotation になるようにローカル回転を書き換える
	/// 届かない目標は、届く範囲に収める。関節の曲がる向きは今の姿勢の向きを使う
	/// </summary>
	/// <param name="skeleton">ボーンの親子関係</param>
	/// <param name="worldMatrices">今の姿勢の各ボーンのワールド行列</param>
	/// <param name="endBone">先端のボーン(手首・足首など)</param>
	/// <param name="targetPosition">目標の位置(ワールド座標)</param>
	/// <param name="targetRotation">先端の目標の向き(ワールドのクォータニオン)</param>
	/// <param name="inOutPose">姿勢(根元・中間・先端の回転が書き換わる)</param>
	/// <param name="outRootBone">書き換えた根元のボーン</param>
	/// <param name="outMidBone">書き換えた中間のボーン</param>
	/// <param name="polePosition">ポールベクトルの目標(ワールド)。ひざ・ひじがこの点の方へ曲がる。nullptrなら今の曲がる向きのまま</param>
	/// <returns>解けたか(チェーンが作れないときなどはfalse)</returns>
	bool Solve(const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& worldMatrices,
		int endBone, const DirectX::XMFLOAT3& targetPosition, const DirectX::XMFLOAT4& targetRotation,
		Pose& inOutPose, int& outRootBone, int& outMidBone,
		const DirectX::XMFLOAT3* polePosition = nullptr);

	/// <summary>
	/// 今の姿勢から、ポールベクトルの目標の初期位置を求める
	/// (中間の関節から、今曲がっている方向へ少し離した点)
	/// </summary>
	/// <returns>求められたか(チェーンが作れないときはfalse)</returns>
	bool ComputeDefaultPole(const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& worldMatrices,
		int endBone, DirectX::XMFLOAT3& outPolePosition);
	/// <summary>
/// 先端のボーンからIKのチェーン(先端 → 中間(親) → 根元(親の親))を探す
/// 根元と中間は、子が1本だけのボーンに限る
/// (Hipsのように子が複数あるボーンを回すと、ほかの部位まで動いてしまうため)
/// </summary>
/// <returns>チェーンが作れたか</returns>
	bool FindChain(const Skeleton& skeleton, int endBone, int& outRootBone, int& outMidBone);
}