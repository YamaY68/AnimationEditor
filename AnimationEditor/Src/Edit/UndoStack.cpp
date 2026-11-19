#include "UndoStack.h"
#include<utility>

void UndoStack::Record(int clipIndex, const AnimationClip& clip)
{
	undo_.push_back({ clipIndex, clip.offsetTracks_ });
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
		//今の状態を反対側の履歴へ積んでから復元
		to.push_back({ snap.clipIndex, clip.offsetTracks_ });
		clip.offsetTracks_ = std::move(snap.offsetTracks);

		outClipIndex = snap.clipIndex;
		return true;
	}
	return false;
}