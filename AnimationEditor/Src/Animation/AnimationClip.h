#pragma once
#include<string>
#include<vector>
#include<cmath>
#include"AnimationTrack.h"
#include"Skeleton.h"
#include"Pose.h"

class AnimationClip
{
public:
	AnimationClip(void) = default;
	AnimationClip(const std::string& name, float duration)
		:name_(name), duration_(duration) {
	}

	//元のトラックを取得(インポート時のみ使用。編集では書き換えない)
	BoneTrack* GetOrCreateTrack(int boneIndex);

	//修正分のトラックを取得
	BoneTrack* GetOrCreateOffsetTrack(int boneIndex);
	const BoneTrack* FindOffsetTrack(int boneIndex)const;

	//元＋修正分の姿勢をサンプリング(表示用)
	void Sample(float time, const Skeleton& skeleton, Pose& outPose)const;
	//元の姿勢だけをサンプリング
	void SampleBase(float time, const Skeleton& skeleton, Pose& outPose)const;
	//指定ボーンの元の姿勢だけを取得
	Transform SampleBaseBone(int boneIndex, float time, const Skeleton& skeleton)const;

	//目標の姿勢と元の姿勢の差を、修正分のキーとして打つ
	void SetOffsetKey(int boneIndex, float time, const Transform& targetPose, const Skeleton& skeleton);
	//区間の修正分を置き換える(In点・Out点の姿勢から差を計算)
	void ReplaceOffsetRange(int boneIndex, float startTime, float endTime,
		Transform startPose, Transform endPose, const Skeleton& skeleton);
	//指定時刻に修正分のキーがあるか
	bool HasOffsetKey(int boneIndex, float time)const;
	//修正分のキーを削除
	bool RemoveOffsetKey(int boneIndex, float time);
	//修正0(元の動きのまま)のキーを打つ
	void SetZeroOffsetKey(int boneIndex, float time);

	//元＋修正分の合成
	static Transform Combine(const Transform& base, const Transform& offset);
	//目標－元の差分
	static Transform Difference(const Transform& base, const Transform& target);

	std::string name_;
	float duration_ = 0.0f;
	std::vector<BoneTrack> boneTracks_;    //元のトラック
	std::vector<BoneTrack> offsetTracks_;  //修正分のトラック

private:
	float WrapTime(float time)const;
	static const BoneTrack* FindTrack(const std::vector<BoneTrack>& tracks, int boneIndex);
};