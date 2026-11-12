#pragma once
#include<string>
#include<vector>
#include<unordered_map>
#include<DirectXMath.h>
#include"../Common/Transform.h"
#include"Pose.h"

struct Bone
{
	std::string name_;	//ボーン名
	int parentIndex_ = -1;	//親ボーンインデックス
	Transform bindPoseLocal_;	//初期バインドポーズ
	DirectX::XMFLOAT4X4 invBindMatrix_;	//スキニング用逆バインド行列
};

class Skeleton
{
public:
	Skeleton(void) = default;

	/// <summary>
	/// 新しボーンを追加
	/// </summary>
	/// <param name="name">ボーン名</param>
	/// <param name="parentIndex">親ボーンインデックス</param>
	/// <param name="localBindPose">初期バインド時のローカルTransform</param>
	/// <returns>追加されたボーンのインデクス</returns>
	int AddBone(const std::string& name, int parentIndex, const Transform& localBindPose);

	/// <summary>
	/// ボーン名からインデックスを探索
	/// </summary>
	/// <param name="name">探索するボーン名</param>
	/// <returns>ボーンインデックス</returns>
	int FindBoneIndex(const std::string& name)const;

	// 初期バインドポーズを生成
	Pose CreateBindPose(void)const;

	//スキニングに必要な全ボーンの逆バインド行列を保存
	void ComputeInverseBindMatrices(void);

	/// <summary>
	/// ローカル姿勢から階層をたどって全ボーンのワールド変換行列を算出
	/// </summary>
	/// <param name="localPose">各ボーンのローカルTransform配列</param>
	/// <param name="outWorldMatrices">算出されたワールド行列４ｘ４</param>
	void ComputeWorldMatrices(const Pose& localPose, std::vector<DirectX::XMFLOAT4X4>& outWorldMatrices)const;
	
	//ボーンの骨格と関節球の描画
	void DrawdebugBones(const std::vector<DirectX::XMFLOAT4X4>& worldMatrices, int boneColor, int jointColor)const;

	size_t GetBoneCount(void)const { return bones_.size(); }
	const Bone& GetBone(size_t index)const { return bones_[index]; }

private:
	std::vector<Bone>bones_;
	std::unordered_map<std::string, int>nameToIndex_;

};

