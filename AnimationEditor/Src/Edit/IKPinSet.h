#pragma once
#include<vector>
#include<DirectXMath.h>
#include"../Animation/Skeleton.h"
#include"../Animation/Pose.h"

/// <summary>
/// IKのピン(先端のボーンを、ワールドの位置と向きで固定する)
/// </summary>
struct IKPin
{
	int endBone = -1;									//固定する先端のボーン
	DirectX::XMFLOAT3 position = { 0.0f, 0.0f, 0.0f };	//固定する位置(ワールド)
	DirectX::XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };	//固定する向き(ワールド)
};

/// <summary>
/// ピン留めしたIKの一覧
/// ほかのボーン(腰など)を動かしたあとにApplyを呼ぶと、先端がピンの位置と向きに残るように解き直す
/// </summary>
class IKPinSet
{
public:
	//このボーンがピン留めされているか
	bool Has(int endBone)const;
	//今の先端の位置と向きでピン留めする。すでにピン留めされていれば外す
	//戻り値:ピン留めしたらtrue、外したらfalse
	bool Toggle(int endBone, const std::vector<DirectX::XMFLOAT4X4>& worldMatrices);
	//ピンの位置と向きを書き換える(ピン留めした先端をIKで動かしたとき)
	void Set(int endBone, const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT4& rotation);
	//すべてのピンを外す
	void Clear(void) { pins_.clear(); }
	bool IsEmpty(void)const { return pins_.empty(); }

	/// <summary>
	/// すべてのピンについて、先端がピンの位置と向きになるようIKを解き直す
	/// </summary>
	/// <returns>回転を書き換えたボーン</returns>
	std::vector<int> Apply(const Skeleton& skeleton, Pose& inOutPose)const;

	const std::vector<IKPin>& GetPins(void)const { return pins_; }

private:
	std::vector<IKPin> pins_;
};