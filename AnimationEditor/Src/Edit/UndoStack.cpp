#include "UndoStack.h"
#include<utility>

UndoStack::Snapshot UndoStack::MakeSnapshot(int clipIndex, const AnimationClip& clip, bool includeBase)
{
	Snapshot snap;
	snap.clipIndex = clipIndex;
	snap.offsetTracks = clip.offsetTracks_;
	snap.hasBase = includeBase;
	if (includeBase)
	{
		snap.boneTracks = clip.boneTracks_;
		snap.duration = clip.duration_;
	}
	return snap;
}

void UndoStack::Record(int clipIndex, const AnimationClip& clip, bool includeBase)
{
	undo_.push_back(MakeSnapshot(clipIndex, clip, includeBase));
	//上限を超えたら一番古い履歴を捨てる
	if (undo_.size() > kMaxHistory)
	{
		undo_.erase(undo_.begin());
	}
	//新しい編集をしたら、やり直し履歴は無効になる
	redo_.clear();
}

bool UndoStack::Undo(std::vector<AnimationClip>& clips, int& outClipIndex)
{
	return Restore(undo_, redo_, clips, outClipIndex);
}

bool UndoStack::Redo(std::vector<AnimationClip>& clips, int& outClipIndex)
{
	return Restore(redo_, undo_, clips, outClipIndex);
}

bool UndoStack::Restore(std::vector<Snapshot>& from, std::vector<Snapshot>& to,
	std::vector<AnimationClip>& clips, int& outClipIndex)
{
	while (!from.empty())
	{
		Snapshot snap = std::move(from.back());
		from.pop_back();

		//クリップが存在しない履歴は捨てて次へ
		if (snap.clipIndex < 0 || snap.clipIndex >= static_cast<int>(clips.size()))
		{
			continue;
		}

		AnimationClip& clip = clips[snap.clipIndex];
		//今の状態を反対側の履歴へ積んでから復元(元の動きも保存していた履歴なら、元の動きも積む)
		to.push_back(MakeSnapshot(snap.clipIndex, clip, snap.hasBase));
		clip.offsetTracks_ = std::move(snap.offsetTracks);
		if (snap.hasBase)
		{
			clip.boneTracks_ = std::move(snap.boneTracks);
			clip.duration_ = snap.duration;
		}

		outClipIndex = snap.clipIndex;
		return true;
	}
	return false;
}