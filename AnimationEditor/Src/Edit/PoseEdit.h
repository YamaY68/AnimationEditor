#pragma once
#include"EditorState.h"
#include"../Animation/Skeleton.h"
#include"../Common/Transform.h"

/// <summary>
/// 姿勢の編集の共通処理(インスペクターとギズモで使う)
/// </summary>
namespace PoseEdit
{
	/// <summary>
	/// 姿勢を変えたときの処理
	/// オートキーがONなら修正分のキーを打つ(ドラッグ1回につきUndoの履歴は1つだけ積む)
	/// </summary>
	/// <returns>キーを打ったか</returns>
	bool OnPoseChanged(const Transform& trs, int boneIndex, const Skeleton& skeleton, EditorState& state);
}