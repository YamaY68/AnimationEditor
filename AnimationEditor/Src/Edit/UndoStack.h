#pragma once
#include<vector>
#include"../Animation/AnimationClip.h"

//修正分トラックのスナップショットによるUndo/Redo
class UndoStack
{
public:
	//編集の「直前」に呼んで、今の状態を履歴に積む
	void Record(int clipIndex, const AnimationClip& clip);

	//元に戻す / やり直す(戻したクリップの番号をoutClipIndexに返す)
	bool Undo(std::vector<AnimationClip>& clips, int& outClipIndex);
	bool Redo(std::vector<AnimationClip>& clips, int& outClipIndex);

	void Clear(void) { undo_.clear(); redo_.clear(); }

private:
	struct Snapshot
	{
		int clipIndex = -1;
		std::vector<BoneTrack> offsetTracks;
	};

	//fromから1つ取り出して復元し、復元前の状態をtoへ積む
	static bool Restore(std::vector<Snapshot>& from, std::vector<Snapshot>& to,
		std::vector<AnimationClip>& clips, int& outClipIndex);

	std::vector<Snapshot> undo_;
	std::vector<Snapshot> redo_;
	static constexpr size_t kMaxHistory = 100;
};