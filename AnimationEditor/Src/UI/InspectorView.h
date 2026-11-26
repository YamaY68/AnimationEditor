#pragma once
#include<DirectXMath.h>
#include"../Animation/Skeleton.h"
#include"../Animation/Pose.h"
#include"../Edit/EditorState.h"

/// <summary>
/// インスペクターの操作結果
/// </summary>
struct InspectorResult
{
	bool keyEdited = false;		//キーを打った・消した・変更した(ポーズを計算し直す必要がある)
	bool poseEdited = false;	//姿勢をいじった(再生を止める必要がある)
};

/// <summary>
/// インスペクターウィンドウ
/// ボーンの階層表示と選択、選択中のボーンの姿勢の編集、キーの操作を行う
/// </summary>
class InspectorView
{
public:
	InspectorView(void) = default;
	~InspectorView(void) = default;

	/// <summary>
	/// インスペクターを描画し、ボーンの選択や姿勢・キーの編集を行う
	/// </summary>
	/// <param name="skeleton">ボーンの階層やバインドポーズ</param>
	/// <param name="inOutPose">今表示している姿勢(編集するとここが書き換わる)</param>
	/// <param name="state">エディタの状態(選択中のボーン・クリップ・Undoなどを読み書きする)</param>
	InspectorResult Draw(const Skeleton& skeleton, Pose& inOutPose, EditorState& state);

private:
	//ボーンの階層ツリー
	void DrawBoneHierarchy(const Skeleton& skeleton, EditorState& state);
	void DrawBoneNode(int boneIndex, const Skeleton& skeleton, EditorState& state);

	//選択中のボーンの位置・回転・スケールの編集欄
	void DrawTransformEditor(Transform& trs, int boneIndex, const Skeleton& skeleton,
		EditorState& state, InspectorResult& result);
	//キーを打つ・消す・補間方法・影響範囲の限定
	void DrawKeyControls(const Transform& trs, int boneIndex, const Skeleton& skeleton,
		AnimationClip& clip, EditorState& state, InspectorResult& result);
	//区間置換(In点～Out点)
	void DrawRangeReplace(const Transform& trs, int boneIndex, const Skeleton& skeleton,
		AnimationClip& clip, EditorState& state, InspectorResult& result);

	//ベジェ曲線の編集欄(グラフ・プリセット・数値)
	void DrawBezierEditor(int boneIndex, float time, AnimationClip& clip, EditorState& state, InspectorResult& result);

	//姿勢をいじったときの共通処理(オートキーがONならキーを打つ)
	void OnPoseChanged(const Transform& trs, int boneIndex, const Skeleton& skeleton,
		EditorState& state, InspectorResult& result);

	//---------- 区間置換 ----------
	float rangeStartTime_ = 0.0f;	//In点の時刻
	Transform rangeStartTrs_;		//In点の姿勢
	int rangeStartBone_ = -1;		//In点を記録したボーン(-1:未記録)

	//---------- 影響範囲の限定 ----------
	float limitRangeSec_ = 0.3f;	//Limit Edit Rangeで0キーを打つ前後の幅(秒)

	//---------- オートキー ----------
	bool editRecorded_ = false;		//今のドラッグで、Undoの履歴をもう積んだか

	//---------- ヒエラルキーの自動展開 ----------
	int lastSelectedBone_ = -1;	//前のフレームでツリーが表示していた選択中のボーン
	int revealBone_ = -1;		//このフレームで開いてスクロールするボーン(-1:しない)

	//---------- 回転の表示 ----------
	DirectX::XMFLOAT3 eulerCache_ = { 0.0f, 0.0f, 0.0f };			//表示中のオイラー角(度)
	DirectX::XMFLOAT4 eulerCacheQuat_ = { 0.0f, 0.0f, 0.0f, 1.0f };	//そのオイラー角から作ったクォータニオン
	int eulerCacheBone_ = -1;										//どのボーンの値か
};