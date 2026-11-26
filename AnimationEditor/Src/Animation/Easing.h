#pragma once
#include<algorithm>
#include<cmath>

/// <summary>
/// キーから次のキーまでの補間方法
/// </summary>
enum class InterpolationType
{
	Linear,		//一定の速さで変化する
	Step,		//次のキーまで値を保ち、キーの位置で切り替わる
	EaseIn,		//ゆっくり動き出す(2次曲線)
	EaseOut,	//ゆっくり止まる(2次曲線)
	EaseInOut,	//ゆっくり動き出して、ゆっくり止まる
	Bezier,		//2つの制御点で決めた曲線(CSSのcubic-bezierと同じ)
};

/// <summary>
/// ベジェ補間の制御点
/// 曲線は (0,0) → (x1,y1) → (x2,y2) → (1,1) の3次ベジェ
/// x:キー間の時間の進み具合  y:値の進み具合
/// xは0～1に収める(時間が戻らないように)。yは0～1の外に出してもよい(行き過ぎて戻る動きになる)
/// </summary>
struct BezierHandles
{
	float x1 = 0.42f;
	float y1 = 0.0f;
	float x2 = 0.58f;
	float y2 = 1.0f;
};

namespace Easing
{
	/// <summary>
	/// 3次ベジェ曲線の1つの軸の値(始点0・終点1)
	/// </summary>
	/// <param name="s">曲線のパラメータ(0～1)</param>
	inline float BezierAxis(float s, float p1, float p2)
	{
		const float u = 1.0f - s;
		return 3.0f * u * u * s * p1 + 3.0f * u * s * s * p2 + s * s * s;
	}

	/// <summary>
	/// ベジェ曲線で、時間の進み具合xに対する値の進み具合yを求める
	/// (x(s) = x となる s をニュートン法で求め、y(s) を返す)
	/// </summary>
	inline float EvaluateBezier(float x, const BezierHandles& h)
	{
		constexpr int kNewtonIterations = 8;
		constexpr int kBisectionIterations = 20;
		constexpr float kTolerance = 1e-5f;

		const float x1 = std::clamp(h.x1, 0.0f, 1.0f);
		const float x2 = std::clamp(h.x2, 0.0f, 1.0f);

		//ニュートン法(ほとんどの場合、数回で収束する)
		float s = x;
		for (int i = 0; i < kNewtonIterations; ++i)
		{
			const float error = BezierAxis(s, x1, x2) - x;
			if (std::abs(error) < kTolerance) return BezierAxis(s, h.y1, h.y2);
			//x(s)の傾き
			const float u = 1.0f - s;
			const float slope = 3.0f * u * u * x1 + 6.0f * u * s * (x2 - x1) + 3.0f * s * s * (1.0f - x2);
			if (std::abs(slope) < kTolerance) break;
			s -= error / slope;
		}

		//収束しなかったら二分法(x(s)は単調に増えるので必ず見つかる)
		float lo = 0.0f;
		float hi = 1.0f;
		s = x;
		for (int i = 0; i < kBisectionIterations; ++i)
		{
			const float value = BezierAxis(s, x1, x2);
			if (std::abs(value - x) < kTolerance) break;
			if (value < x) lo = s; else hi = s;
			s = (lo + hi) * 0.5f;
		}
		return BezierAxis(s, h.y1, h.y2);
	}

	/// <summary>
/// EvaluateBezierの逆(値の進み具合yになる、時間の進み具合xを求める)
/// 曲線が単調に増えるとき(制御点のyが0～1のとき)だけ使える
/// </summary>
	inline float InverseBezier(float y, const BezierHandles& h)
	{
		constexpr int kIterations = 30;
		float lo = 0.0f;
		float hi = 1.0f;
		for (int i = 0; i < kIterations; ++i)
		{
			const float mid = (lo + hi) * 0.5f;
			if (EvaluateBezier(mid, h) < y) lo = mid; else hi = mid;
		}
		return (lo + hi) * 0.5f;
	}


	/// <summary>
	/// 補間の進み具合を、補間方法に応じた曲線で変換する
	/// </summary>
	/// <param name="t">キー間の進み具合(0:前のキー ～ 1:次のキー)</param>
	/// <param name="bezier">Bezierのときの制御点</param>
	/// <returns>変換後の進み具合(基本は0～1。Bezierでは外に出ることもある)</returns>
	inline float Evaluate(float t, InterpolationType type, const BezierHandles& bezier = BezierHandles{})
	{
		t = std::clamp(t, 0.0f, 1.0f);

		switch (type)
		{
		case InterpolationType::Step:
			return 0.0f;
		case InterpolationType::EaseIn:
			return t * t;
		case InterpolationType::EaseOut:
			return t * (2.0f - t);
		case InterpolationType::EaseInOut:
			return (t < 0.5f) ? (2.0f * t * t) : (-1.0f + (4.0f - 2.0f * t) * t);
		case InterpolationType::Bezier:
			return EvaluateBezier(t, bezier);
		case InterpolationType::Linear:
		default:
			return t;
		}
	}
}