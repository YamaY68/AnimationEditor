#pragma once
#include<vector>
#include"../Animation/AnimationClip.h"

//修正分トラックのスナップショットによるUndo/Redo
//(速度変更のように元の動きまで書き換える編集では、元の動きと長さも一緒に保存する)
class UndoStack
{
public:
	//編集の「直前」に呼んで、今の状態を履歴に積む
	//includeBase:元の動き(boneTracks_)と長さも保存するか(重いので、元の動きを書き換えるときだけ)
	void Record(int clipIndex, const AnimationClip& clip, bool includeBase = false);

	//元に戻す / やり直す(戻したクリップの番号をoutClipIndexに返す)
	bool Undo(std::vector<AnimationClip>& clips, int& outClipIndex);
	bool Redo(std::vector<AnimationClip>& clips, int& outClipIndex);

	void Clear(void) { undo_.clear(); redo_.clear(); }

private:
	struct Snapshot
	{
		int clipIndex = -1;
		std::vector<BoneTrack> offsetTracks;
		bool hasBase = false;				//元の動きと長さも保存しているか
		std::vector<BoneTrack> boneTracks;	//元の動き(hasBaseのときだけ)
		float duration = 0.0f;				//長さ(hasBaseのときだけ)
	};

	//クリップの今の状態をスナップショットにする
	static Snapshot MakeSnapshot(int clipIndex, const AnimationClip& clip, bool includeBase);

	//fromから1つ取り出して復元し、復元前の状態をtoへ積む
	static bool Restore(std::vector<Snapshot>& from, std::vector<Snapshot>& to,
		std::vector<AnimationClip>& clips, int& outClipIndex);

	std::vector<Snapshot> undo_;
	std::vector<Snapshot> redo_;
	static constexpr size_t kMaxHistory = 100;
};