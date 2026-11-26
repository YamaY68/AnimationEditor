#pragma once
#include<vector>
#include<DirectXMath.h>
#include"../Animation/Skeleton.h"
#include"../Animation/Pose.h"

/// <summary>
/// 選択中のボーンを動かすギズモ(ImGuizmoを使用)
/// </summary>
class BoneGizmo
{
public:
	//操作の種類
	enum class Operation { Translate, Rotate, IK };
	//軸の向き(Local:ボーン自身の向き World:ワールドの向き)
	enum class Space { Local, World };

	/// <summary>
	/// 選択中のボーンにギズモを描き、ドラッグされたら姿勢を書き換える
	/// </summary>
	/// <param name="skeleton">ボーンの親子関係</param>
	/// <param name="worldMatrices">各ボーンのワールド行列</param>
	/// <param name="inOutPose">今表示している姿勢(ドラッグするとここが書き換わる)</param>
	/// <param name="boneIndex">選択中のボーン(-1ならギズモを出さない)</param>
	/// <returns>姿勢を書き換えたボーン(書き換えていなければ空)</returns>
	std::vector<int> Draw(const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& worldMatrices,
		Pose& inOutPose, int boneIndex);

	//操作を切り替える(IKに入るときは、IK移動から始める)
	void SetOperation(Operation op);
	//右クリック用の切り替え
	//通常:移動 ⇔ 回転  IKモード中:IK移動 ⇔ IK回転
	void ToggleOperation(void);
	void ToggleSpace(void) { space_ = (space_ == Space::Local) ? Space::World : Space::Local; }
	Operation GetOperation(void)const { return operation_; }
	//IKの目標の位置と向き(ワールド)
	const DirectX::XMFLOAT3& GetIKTargetPosition(void)const { return ikTargetPosition_; }
	const DirectX::XMFLOAT4& GetIKTargetRotation(void)const { return ikTargetRotation_; }
	//マウスがギズモの上にあるか
	bool IsHovered(void)const;
	//ギズモをドラッグ中か
	bool IsUsing(void)const;

private:
	//今のモードを画面の左上に表示する
	void DrawStatus(void)const;

	bool drawn_ = false;	//このフレームでギズモを描いたか
	Operation operation_ = Operation::Translate;
	Space space_ = Space::Local;

	//---------- IK ----------
	bool ikRotate_ = false;	//IKモードで、目標の向きを回すか(false:位置を動かす)
	DirectX::XMFLOAT3 ikTargetPosition_ = { 0.0f, 0.0f, 0.0f };			//目標の位置(ワールド)
	DirectX::XMFLOAT4 ikTargetRotation_ = { 0.0f, 0.0f, 0.0f, 1.0f };	//先端の目標の向き(ワールド)
};