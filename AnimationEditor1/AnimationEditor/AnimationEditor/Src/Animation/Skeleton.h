#pragma once
#include<string>
#include<vector>
#include<unordered_map>
#include<DirectXMath.h>
#include"../Common/Transform.h"
#include"Pose.h"

struct Bone
{
	std::string name_;
	int parentIndex_ = -1;
	Transform bindPoseLocal_;
	DirectX::XMFLOAT4X4 invBindMatrix_;	//スキニング用逆バインド行列
};

class Skeleton
{
public:
	Skeleton(void) = default;
	~Skeleton(void) = default;

	int AddBone(const std::string&name,int parentIndex,const Transform& localBindPose);

	int FindBoneIndex(const std::string& name)const;

	size_t GetBoneCount(void)const { return bones_.size(); }

	const Bone& GetBone(size_t index)const { return bones_[index]; }

	const std::vector<Bone>& GetBone(void)const { return bones_; }

	Pose CreateBindPose(void)const;

	void ComputeInverseBindMatrices(void);

	void ComputeWorldMatrices(const Pose& localPose, 
		std::vector<DirectX::XMFLOAT4X4>& outWorldMatrices) const;
	
	void DrawDebugBones(const std::vector<DirectX::XMFLOAT4X4>& worldMatrices, int boneColor, int jointColor)const;

	Bone& GetBone(size_t index) { return bones_[index]; }
	std::vector<Bone>& GetBone(void) { return bones_; }

private:
	std::unordered_map<std::string, int>nameToIndex_;
	std::vector<Bone>bones_;
};

