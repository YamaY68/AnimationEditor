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
	//IKモードの中で何を動かすか(Move:目標の位置 Rotate:目標の向き Pole:ひざ・ひじの向き)
	enum class IKMode { Move, Rotate, Pole };

	/// <summary>
	/// 選択中のボーンにギズモを描き、ドラッグされたら姿勢を書き換える
	/// </summary>
	/// <param name="skeleton">ボーンの親子関係</param>
	/// <param name="worldMatrices">各ボーンのワールド行列</param>
	/// <param name="inOutPose">今表示している姿勢(ドラッグするとここが書き換わる)</param>
	/// <param name="boneIndex">選択中のボーン(-1ならギズモを出さない)</param>
	/// <param name="pinnedPole">ピンに固定されたポールの位置(無ければnullptr。今の曲がる向きから求める)</param>
	/// <returns>姿勢を書き換えたボーン(書き換えていなければ空)</returns>
	std::vector<int> Draw(const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& worldMatrices,
		Pose& inOutPose, int boneIndex, const DirectX::XMFLOAT3* pinnedPole = nullptr);

	//操作を切り替える(IKに入るときは、IK移動から始める)
	void SetOperation(Operation op);
	//右クリック用の切り替え
	//通常:移動 ⇔ 回転  IKモード中:IK移動 → IK回転 → ポール → IK移動
	void ToggleOperation(void);
	void ToggleSpace(void) { space_ = (space_ == Space::Local) ? Space::World : Space::Local; }
	Operation GetOperation(void)const { return operation_; }
	//IKの目標の位置と向き(ワールド)
	const DirectX::XMFLOAT3& GetIKTargetPosition(void)const { return ikTargetPosition_; }
	const DirectX::XMFLOAT4& GetIKTargetRotation(void)const { return ikTargetRotation_; }
	IKMode GetIKMode(void)const { return ikMode_; }
	//ポールの目標の位置(ワールド)
	const DirectX::XMFLOAT3& GetIKPolePosition(void)const { return ikPolePosition_; }
	//このフレームでIKのギズモを出したか(ポールの目印を描くかどうか)
	bool IsIKActive(void)const { return drawn_ && operation_ == Operation::IK; }
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
	IKMode ikMode_ = IKMode::Move;	//IKモードで何を動かすか
	DirectX::XMFLOAT3 ikTargetPosition_ = { 0.0f, 0.0f, 0.0f };			//目標の位置(ワールド)
	DirectX::XMFLOAT4 ikTargetRotation_ = { 0.0f, 0.0f, 0.0f, 1.0f };	//先端の目標の向き(ワールド)
	DirectX::XMFLOAT3 ikPolePosition_ = { 0.0f, 0.0f, 0.0f };			//ポールの目標の位置(ワールド)
};