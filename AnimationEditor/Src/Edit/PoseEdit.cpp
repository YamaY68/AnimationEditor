#include "PoseEdit.h"

bool PoseEdit::OnPoseChanged(const Transform& trs, int boneIndex, const Skeleton& skeleton, EditorState& state)
{
	AnimationClip* clip = state.GetActiveClip();
	if (!state.autoKey || clip == nullptr) return false;

	//ドラッグ1回につき、Undoの履歴は1つだけ積む
	if (!state.editRecorded)
	{
		state.RecordUndo();
		state.editRecorded = true;
	}
	clip->SetOffsetKey(boneIndex, state.playback.time, trs, skeleton);
	return true;
}