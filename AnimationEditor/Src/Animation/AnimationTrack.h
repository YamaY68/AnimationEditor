#pragma once
#include<vector>
#include<algorithm>
#include<cmath>
#include"KeyFrame.h"
#include"Easing.h"
#include"../Common/Transform.h"
#include"../Common/EditorConstants.h"

/// <summary>
/// 1つの値(移動・回転・スケールのどれか)のキーフレーム列
/// キーは常に時刻順に並べておく(Sampleで二分探索するため)
/// </summary>
/// <typeparam name="T">値の型(XMFLOAT3 または XMFLOAT4)</typeparam>
template <typename T>
class AnimationTrack
{
public:
	//時刻順に並んだキー
	std::vector<KeyFrame<T>> keys_;

	/// <summary>
	/// キーを追加する(時刻順を保つ)
	/// 同じ時刻のキーがあるかは確認しないので、上書きしたいときはSetKeyを使う
	/// </summary>
	void AddKey(float time, const T& value, InterpolationType interp = InterpolationType::Linear)
	{
		//末尾のキーより後ろなら、末尾に足すだけで順番が保たれる
		//(FBX読み込み時は時刻順に追加されるので、毎回のソートを省ける)
		const bool keepsOrder = keys_.empty() || keys_.back().time_ <= time;
		keys_.emplace_back(time, value, interp);
		if (!keepsOrder)
		{
			std::sort(keys_.begin(), keys_.end(), [](const KeyFrame<T>& a, const KeyFrame<T>& b)
				{
					return a.time_ < b.time_;
				});
		}
	}

	/// <summary>
	/// 指定時刻のキーを探す
	/// </summary>
	/// <returns>見つかったキー(無ければnullptr)</returns>
	const KeyFrame<T>* FindKey(float time)const
	{
		auto it = std::find_if(keys_.begin(), keys_.end(), [time](const KeyFrame<T>& k)
			{
				return std::abs(k.time_ - time) < EditorConst::kKeyTimeEpsilon;
			});
		return (it != keys_.end()) ? &(*it) : nullptr;
	}
	KeyFrame<T>* FindKey(float time)
	{
		//const版と同じ処理なので、const版を呼んで結果のconstを外す
		return const_cast<KeyFrame<T>*>(static_cast<const AnimationTrack&>(*this).FindKey(time));
	}

	/// <summary>
	/// 指定時刻のキーの値を上書きする。無ければ新しく追加する
	/// 既存のキーの補間方法は変えない(オートキーで上書きしたときに補間方法が戻らないように)
	/// </summary>
	/// <param name="interpForNewKey">新しく追加するときの補間方法</param>
	void SetKey(float time, const T& value, InterpolationType interpForNewKey = InterpolationType::Linear)
	{
		if (KeyFrame<T>* key = FindKey(time))
		{
			key->value_ = value;
			return;
		}
		AddKey(time, value, interpForNewKey);
	}

	/// <summary>
	/// 指定時刻のキーを削除する
	/// </summary>
	/// <returns>削除したらtrue</returns>
	bool RemoveKey(float time)
	{
		const size_t oldSize = keys_.size();
		keys_.erase(std::remove_if(keys_.begin(), keys_.end(), [time](const KeyFrame<T>& k)
			{
				return std::abs(k.time_ - time) < EditorConst::kKeyTimeEpsilon;
			}), keys_.end());
		return keys_.size() != oldSize;
	}

	/// <summary>
/// 指定時刻のキーを別の時刻へ移す(移動先にキーがあれば上書きする)
/// </summary>
/// <returns>移したらtrue</returns>
	bool MoveKey(float fromTime, float toTime)
	{
		const KeyFrame<T>* key = FindKey(fromTime);
		if (key == nullptr) return false;

		//消す前に中身を取っておく
		const KeyFrame<T> moved = *key;
		RemoveKey(fromTime);
		RemoveKey(toTime);	//移動先にあったキーは上書きする
		AddKey(toTime, moved.value_, moved.interpolation_);
		FindKey(toTime)->bezier_ = moved.bezier_;	//ベジェの曲線も引き継ぐ   ← 足す
		return true;
		return true;
	}

	/// <summary>
	/// 指定時刻のキーの補間方法を取得する
	/// </summary>
	/// <returns>キーが無ければfalse</returns>
	bool GetInterpolation(float time, InterpolationType& out)const
	{
		if (const KeyFrame<T>* key = FindKey(time))
		{
			out = key->interpolation_;
			return true;
		}
		return false;
	}

	/// <summary>
	/// 指定時刻のキーの補間方法を設定する(キーが無ければ何もしない)
	/// </summary>
	void SetInterpolation(float time, InterpolationType type)
	{
		if (KeyFrame<T>* key = FindKey(time))
		{
			key->interpolation_ = type;
		}
	}

	/// <summary>
/// 指定時刻のキーのベジェ曲線を取得する
/// </summary>
/// <returns>キーが無ければfalse</returns>
	bool GetBezier(float time, BezierHandles& out)const
	{
		if (const KeyFrame<T>* key = FindKey(time))
		{
			out = key->bezier_;
			return true;
		}
		return false;
	}

	/// <summary>
	/// 指定時刻のキーのベジェ曲線を設定する(キーが無ければ何もしない)
	/// </summary>
	void SetBezier(float time, const BezierHandles& bezier)
	{
		if (KeyFrame<T>* key = FindKey(time))
		{
			key->bezier_ = bezier;
		}
	}


	/// <summary>
	/// 区間内のキーをすべて消し、始点と終点にキーを打ち直す
	/// </summary>
	/// <param name="rangeStart">始点の時刻(rangeEnd以下であること)</param>
	/// <param name="rangeEnd">終点の時刻</param>
	void SetRangeKeys(float rangeStart, float rangeEnd, const T& startVal, const T& endVal,
		InterpolationType interp = InterpolationType::Linear)
	{
		const float eps = EditorConst::kKeyTimeEpsilon;
		keys_.erase(std::remove_if(keys_.begin(), keys_.end(), [=](const KeyFrame<T>& k)
			{
				return (k.time_ >= rangeStart - eps && k.time_ <= rangeEnd + eps);
			}), keys_.end());
		SetKey(rangeStart, startVal, interp);
		SetKey(rangeEnd, endVal, interp);
	}

	/// <summary>
	/// 指定時刻の値を求める
	/// 最初のキーより前は最初のキーの値、最後のキーより後は最後のキーの値になる
	/// </summary>
	/// <param name="defaultValue">キーが1つも無いときに返す値</param>
	T Sample(float time, const T& defaultValue)const
	{
		if (keys_.empty()) return defaultValue;
		if (keys_.size() == 1 || time <= keys_.front().time_) return keys_.front().value_;
		if (time >= keys_.back().time_) return keys_.back().value_;

		//timeより後ろにある最初のキーを二分探索で探す(＝次のキー)
		auto nextIt = std::upper_bound(keys_.begin(), keys_.end(), time,
			[](float t, const KeyFrame<T>& key)
			{
				return t < key.time_;
			});
		const KeyFrame<T>& nextKey = *nextIt;
		const KeyFrame<T>& prevKey = *(nextIt - 1);

		//前のキーから次のキーまでの進み具合(0～1)
		float span = nextKey.time_ - prevKey.time_;
		float factor = (span > 0.0f) ? ((time - prevKey.time_) / span) : 0.0f;

		//補間方法は「前のキー」に設定されたものを使う
		return KeyFrameMath::Interpolate(prevKey.value_, nextKey.value_, factor, prevKey.interpolation_, prevKey.bezier_);
	}
};

/// <summary>
/// ボーン1本分のトラック(移動・回転・スケールの3本をまとめたもの)
/// 移動・回転・スケールのキーは、常に同じ時刻にまとめて打つ
/// </summary>
struct BoneTrack
{
	int boneIndex_ = -1;	//対象のボーン
	AnimationTrack<DirectX::XMFLOAT3> translationTrack_;
	AnimationTrack<DirectX::XMFLOAT4> rotationTrack_;
	AnimationTrack<DirectX::XMFLOAT3> scaleTrack_;

	/// <summary>
	/// 指定時刻の姿勢を求める
	/// </summary>
	/// <param name="defaultPose">キーが無いときに使う姿勢</param>
	Transform Sample(float time, const Transform& defaultPose)const
	{
		Transform result;
		result.translation_ = translationTrack_.Sample(time, defaultPose.translation_);
		result.rotation_ = rotationTrack_.Sample(time, defaultPose.rotation_);
		result.scale_ = scaleTrack_.Sample(time, defaultPose.scale_);
		return result;
	}

	//指定時刻に姿勢のキーを打つ(既存のキーは上書き)
	void SetKeyframe(float time, const Transform& transform)
	{
		translationTrack_.SetKey(time, transform.translation_);
		rotationTrack_.SetKey(time, transform.rotation_);
		scaleTrack_.SetKey(time, transform.scale_);
	}

	//指定時刻にキーがあるか
	bool HasKeyframe(float time)const
	{
		return translationTrack_.FindKey(time) != nullptr
			|| rotationTrack_.FindKey(time) != nullptr
			|| scaleTrack_.FindKey(time) != nullptr;
	}

	//指定時刻のキーを削除する(どれか1つでも削除したらtrue)
	bool RemoveKeyframe(float time)
	{
		//||だと最初に成功した時点で残りが実行されないので、|で3つとも実行する
		bool removed = translationTrack_.RemoveKey(time);
		removed |= rotationTrack_.RemoveKey(time);
		removed |= scaleTrack_.RemoveKey(time);
		return removed;
	}

	//指定時刻のキーを別の時刻へ移す(どれか1つでも移したらtrue)
	bool MoveKeyframe(float fromTime, float toTime)
	{
		bool moved = translationTrack_.MoveKey(fromTime, toTime);
		moved |= rotationTrack_.MoveKey(fromTime, toTime);
		moved |= scaleTrack_.MoveKey(fromTime, toTime);
		return moved;
	}

	//指定時刻のキーの補間方法を取得する(キーが無ければfalse)
	bool GetInterpolation(float time, InterpolationType& out)const
	{
		return rotationTrack_.GetInterpolation(time, out)
			|| translationTrack_.GetInterpolation(time, out)
			|| scaleTrack_.GetInterpolation(time, out);
	}

	//指定時刻のキーの補間方法を、3本まとめて設定する
	void SetInterpolation(float time, InterpolationType type)
	{
		translationTrack_.SetInterpolation(time, type);
		rotationTrack_.SetInterpolation(time, type);
		scaleTrack_.SetInterpolation(time, type);
	}

	//指定時刻のキーのベジェ曲線を取得する(キーが無ければfalse)
	bool GetBezier(float time, BezierHandles& out)const
	{
		return rotationTrack_.GetBezier(time, out)
			|| translationTrack_.GetBezier(time, out)
			|| scaleTrack_.GetBezier(time, out);
	}

	//指定時刻のキーのベジェ曲線を、3本まとめて設定する
	void SetBezier(float time, const BezierHandles& bezier)
	{
		translationTrack_.SetBezier(time, bezier);
		rotationTrack_.SetBezier(time, bezier);
		scaleTrack_.SetBezier(time, bezier);
	}


	//区間内のキーを消し、始点と終点の姿勢でキーを打ち直す
	void ReplaceRange(float rangeStart, float rangeEnd, const Transform& startPose, const Transform& endPose)
	{
		translationTrack_.SetRangeKeys(rangeStart, rangeEnd, startPose.translation_, endPose.translation_);
		rotationTrack_.SetRangeKeys(rangeStart, rangeEnd, startPose.rotation_, endPose.rotation_);
		scaleTrack_.SetRangeKeys(rangeStart, rangeEnd, startPose.scale_, endPose.scale_);
	}
};