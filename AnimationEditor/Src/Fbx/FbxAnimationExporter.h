#pragma once
#include<string>
#include<vector>
#include"../Animation/Skeleton.h"
#include"../Animation/AnimationClip.h"

/// <summary>
/// 編集したアニメーションをFBXに書き出す
///
/// モデルの元のFBXを開き直し、メッシュ・スキン・マテリアルはそのまま残して、
/// 中のアニメーションだけを編集後の動き(元の動き＋修正分)に差し替えて保存する
/// </summary>
namespace FbxAnimationExporter
{
	/// <summary>
	/// アニメーションをFBXに書き出す
	/// </summary>
	/// <param name="modelPath">モデルの元のFBX(読み込んだときのパス)</param>
	/// <param name="outputPath">保存先(Shift-JISのパス。ファイルダイアログが返すもの)</param>
	/// <param name="skeleton">ボーンの親子関係とバインドポーズ</param>
	/// <param name="clips">書き出すクリップ(1本につきAnimStackを1つ作る)</param>
	/// <param name="outError">失敗したときの理由</param>
	/// <returns>書き出せたか</returns>
	bool Export(const std::string& modelPath, const std::string& outputPath,
		const Skeleton& skeleton, const std::vector<const AnimationClip*>& clips, std::string& outError);
}