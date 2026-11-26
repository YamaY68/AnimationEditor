#pragma once
#include<string>
#include<vector>
#include"AnimationTrack.h"
#include"Skeleton.h"
#include"Pose.h"

/// <summary>
/// アニメーション1本分
///
/// 「元の動き」と「修正分」の2層に分けて持つ(アニメーションレイヤー方式)
///   boneTracks_   : FBXから読み込んだ元の動き。編集では書き換えない
///   offsetTracks_ : ユーザーが打ったキー。元の動きとの差分を持つ
/// 表示する姿勢は「元の動き + 修正分」で求める
/// </summary>
class AnimationClip
{
public:
	AnimationClip(void) = default;
	AnimationClip(const std::string& name, float duration)
		:name_(name), duration_(duration) {
	}

	//---------- トラックの取得 ----------

	//元の動きのトラックを取得(無ければ作る)。FBX読み込み時だけ使う
	BoneTrack* GetOrCreateTrack(int boneIndex);
	//修正分のトラックを取得(無ければ作る)
	BoneTrack* GetOrCreateOffsetTrack(int boneIndex);
	//修正分のトラックを探す(無ければnullptr)
	const BoneTrack* FindOffsetTrack(int boneIndex)const;

	//---------- 姿勢の計算 ----------

	//元の動き＋修正分の姿勢を求める(画面に表示する姿勢)
	void Sample(float time, const Skeleton& skeleton, Pose& outPose)const;
	//元の動きだけの姿勢を求める
	void SampleBase(float time, const Skeleton& skeleton, Pose& outPose)const;
	//指定ボーンの、元の動きだけの姿勢を求める
	Transform SampleBaseBone(int boneIndex, float time, const Skeleton& skeleton)const;

	//---------- 修正分のキーの編集 ----------

	//targetPoseになるように、元の動きとの差を修正分のキーとして打つ
	void SetOffsetKey(int boneIndex, float time, const Transform& targetPose, const Skeleton& skeleton);
	//区間内の修正分のキーを消し、始点と終点の姿勢でキーを打ち直す(始点と終点が逆なら入れ替える)
	void ReplaceOffsetRange(int boneIndex, float startTime, float endTime,
		Transform startPose, Transform endPose, const Skeleton& skeleton);
	//指定時刻に修正分のキーがあるか
	bool HasOffsetKey(int boneIndex, float time)const;
	//修正分のキーを削除する(削除したらtrue)
	bool RemoveOffsetKey(int boneIndex, float time);
	//指定時刻の修正分のキーを別の時刻へ移す(boneIndex=-1なら全ボーン)。移したらtrue
	bool MoveOffsetKeys(int boneIndex, float fromTime, float toTime);
	//指定時刻の修正分のキーを削除する(boneIndex=-1なら全ボーン)。削除したらtrue
	bool RemoveOffsetKeys(int boneIndex, float time);
	//修正0(元の動きのまま)のキーを打つ
	void SetZeroOffsetKey(int boneIndex, float time);
	//修正分のキーの補間方法を取得する(キーが無ければfalse)
	bool GetOffsetInterpolation(int boneIndex, float time, InterpolationType& out)const;
	//修正分のキーの補間方法を設定する
	void SetOffsetInterpolation(int boneIndex, float time, InterpolationType type);

	//---------- 元の動きと修正分の計算 ----------

	//元の動き＋修正分
	static Transform Combine(const Transform& base, const Transform& offset);
	//目標の姿勢－元の動き(＝修正分)。Combine(base, Difference(base, target)) == target になる
	static Transform Difference(const Transform& base, const Transform& target);

	std::string name_;						//クリップ名
	float duration_ = 0.0f;					//長さ(秒)
	std::vector<BoneTrack> boneTracks_;		//元の動き
	std::vector<BoneTrack> offsetTracks_;	//修正分
	//---------- 遅延読み込み ----------
//モデルのFBXに入っているアニメーションは、最初は名前と長さだけを読み、選ばれたときに中身を読む
	bool isLoaded_ = true;		//元の動き(boneTracks_)を読み込み済みか
	int sourceStackIndex_ = -1;	//読み込み元のAnimStackの番号
	int sourceSceneId_ = -1;	//読み込み元のシーンの番号(モデルを読み込み直すと変わる)
private:
	//時刻を0～duration_の範囲に折り返す(終端ちょうどは折り返さない)
	float WrapTime(float time)const;

	//トラック配列から指定ボーンのトラックを探す
	static const BoneTrack* FindTrack(const std::vector<BoneTrack>& tracks, int boneIndex);
	static BoneTrack* FindTrack(std::vector<BoneTrack>& tracks, int boneIndex);
	//トラック配列から指定ボーンのトラックを探し、無ければ末尾に作る
	static BoneTrack* GetOrCreate(std::vector<BoneTrack>& tracks, int boneIndex);
};