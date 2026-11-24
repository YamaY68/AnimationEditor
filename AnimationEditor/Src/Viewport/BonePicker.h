#pragma once
#include<vector>
#include<DirectXMath.h>
#include"../Animation/Skeleton.h"

/// <summary>
/// ビューポート上のクリック位置から、ボーンを選ぶ
/// </summary>
namespace BonePicker
{
	/// <summary>
	/// 画面上でマウスに一番近いボーンを探す
	/// 関節(球)を優先し、関節が範囲内に無ければライン(親→子)で判定する
	/// ラインをクリックしたときは、根元(親側)のボーンを選ぶ
	/// </summary>
	/// <param name="skeleton">ボーンの親子関係</param>
	/// <param name="worldMatrices">各ボーンのワールド行列</param>
	/// <param name="mouseX">マウスのX座標(スクリーン座標)</param>
	/// <param name="mouseY">マウスのY座標(スクリーン座標)</param>
	/// <param name="jointRadiusPx">関節を選べる距離(ピクセル)</param>
	/// <param name="lineRadiusPx">ラインを選べる距離(ピクセル)</param>
	/// <returns>見つかったボーンの番号。範囲内に無ければ-1</returns>
	int Pick(const Skeleton& skeleton, const std::vector<DirectX::XMFLOAT4X4>& worldMatrices,
		float mouseX, float mouseY, float jointRadiusPx, float lineRadiusPx);
}