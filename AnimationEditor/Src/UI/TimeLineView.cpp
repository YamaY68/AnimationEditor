#include "TimeLineView.h"
#include<imgui.h>
#include<imgui_internal.h>
#include<cmath>
#include<algorithm>
#include<iterator>
#include<string>
#include"../Common/EditorConstants.h"

namespace
{
	//---------- 見た目の設定 ----------
	constexpr ImVec2 kWindowSize(860.0f, 140.0f);				//ウィンドウの初期サイズ
	constexpr float kMarkerRadius = 5.0f;						//キーマーカーの大きさ
	constexpr float kCurrentMarkerRadius = 7.0f;				//再生位置にあるキーマーカーの大きさ
	constexpr float kMarkerHitRadius = 7.0f;					//キーマーカーのクリック判定の大きさ
	constexpr ImU32 kMarkerColor = IM_COL32(255, 230, 0, 255);			//キーマーカーの色
	constexpr ImU32 kCurrentMarkerColor = IM_COL32(255, 255, 255, 255);	//再生位置にあるキーマーカーの色
	constexpr ImU32 kMarkerOutlineColor = IM_COL32(0, 0, 0, 255);		//キーマーカーの縁の色
	constexpr float kMinSpeed = -2.0f;							//再生速度の最小(マイナスは逆再生)
	constexpr float kMaxSpeed = 3.0f;							//再生速度の最大

	/// <summary>
	/// 修正分のキーの時刻を集める(昇順・重複なし)
	/// </summary>
	/// <param name="boneIndex">このボーンのキーだけ集める(-1なら全ボーン)</param>
	std::vector<float> CollectKeyTimes(const AnimationClip& clip, int boneIndex)
	{
		std::vector<float> keyTimes;
		for (const auto& track : clip.offsetTracks_)
		{
			if (boneIndex != -1 && track.boneIndex_ != boneIndex) continue;

			for (const auto& k : track.translationTrack_.keys_) keyTimes.push_back(k.time_);
			for (const auto& k : track.rotationTrack_.keys_)    keyTimes.push_back(k.time_);
			for (const auto& k : track.scaleTrack_.keys_)       keyTimes.push_back(k.time_);
		}

		//並べて、同じ時刻のキーを1つにまとめる(移動・回転・スケールは同じ時刻に打たれるため)
		std::sort(keyTimes.begin(), keyTimes.end());
		keyTimes.erase(std::unique(keyTimes.begin(), keyTimes.end(), [](float a, float b)
			{
				return std::abs(a - b) < EditorConst::kKeyTimeEpsilon;
			}), keyTimes.end());
		return keyTimes;
	}

	/// <summary>
	/// 次(direction=+1)または前(direction=-1)のキーの時刻を求める。端ではループする
	/// </summary>
	/// <param name="keyTimes">キーの時刻(昇順に並んでいること)</param>
	/// <returns>移動先の時刻(キーが無ければtimeのまま)</returns>
	float FindAdjacentKeyTime(const std::vector<float>& keyTimes, float time, int direction)
	{
		if (keyTimes.empty()) return time;

		const float eps = EditorConst::kKeyTimeEpsilon;
		if (direction > 0)
		{
			//今の時刻より後ろの最初のキー。無ければ最初のキーへ
			auto it = std::upper_bound(keyTimes.begin(), keyTimes.end(), time + eps);
			return (it != keyTimes.end()) ? *it : keyTimes.front();
		}
		//今の時刻より前の最後のキー。無ければ最後のキーへ
		auto it = std::lower_bound(keyTimes.begin(), keyTimes.end(), time - eps);
		return (it != keyTimes.begin()) ? *std::prev(it) : keyTimes.back();
	}

	/// <summary>
	/// 1フレーム進める(direction=+1)または戻す(direction=-1)。端ではループする
	/// 端を越えるときは、いったん端で止まってから反対の端へ移る(最終フレームを確実に見られるように)
	/// </summary>
	float StepFrame(float time, float duration, int direction)
	{
		const float eps = EditorConst::kKeyTimeEpsilon;
		if (direction > 0)
		{
			if (time >= duration - eps) return 0.0f;	//終端にいたら先頭へ
			return std::min(time + EditorConst::kFrameDuration, duration);
		}
		if (time <= eps) return duration;				//先頭にいたら終端へ
		return std::max(time - EditorConst::kFrameDuration, 0.0f);
	}

	/// <summary>
	/// クリップの選択欄と長さの表示
	/// </summary>
	void DrawClipSelector(EditorState& state)
	{
		const AnimationClip& activeClip = state.clips[state.currentClipIndex];

		ImGui::SetNextItemWidth(220.0f);
		if (ImGui::BeginCombo("Active Clip", activeClip.name_.c_str()))
		{
			for (int i = 0; i < static_cast<int>(state.clips.size()); ++i)
			{
				const bool isSelected = (state.currentClipIndex == i);
				//同じ名前のクリップがあってもImGuiのIDが重ならないように、番号を付ける
				const std::string label = state.clips[i].name_ + "##" + std::to_string(i);
				//クリップを切り替えたら、再生時刻を先頭に戻す
				if (ImGui::Selectable(label.c_str(), isSelected))
				{
					state.currentClipIndex = i;
					state.playback.time = 0.0f;
				}
				if (isSelected)
				{
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::Text("| Duration:%.2f sec", state.clips[state.currentClipIndex].duration_);
	}

	/// <summary>
	/// 再生・停止・ループ・速度・オートキーの操作
	/// </summary>
	void DrawPlaybackControls(EditorState& state)
	{
		PlaybackState& playback = state.playback;

		if (ImGui::Button(playback.isPlaying ? "Pause" : "Play", ImVec2(60, 0)))
		{
			playback.isPlaying = !playback.isPlaying;
		}
		ImGui::SameLine();
		if (ImGui::Button("Stop", ImVec2(50, 0)))
		{
			playback.isPlaying = false;
			playback.time = 0.0f;
		}
		ImGui::SameLine();
		ImGui::Checkbox("Loop", &playback.isLoop);

		ImGui::SameLine();
		ImGui::SetNextItemWidth(110.0f);
		ImGui::SliderFloat("Speed", &playback.speed, kMinSpeed, kMaxSpeed, "%.2fx");
		ImGui::SameLine();
		if (ImGui::Button("1.0x"))
		{
			playback.speed = 1.0f;
		}

		ImGui::SameLine();
		ImGui::Checkbox("Auto Key", &state.autoKey);
	}

	/// <summary>
	/// ブレンド再生の設定(クリップが2本以上あるときだけ表示)
	/// </summary>
	void DrawBlendControls(EditorState& state)
	{
		BlendState& blend = state.blend;

		//クリップが1本しかないときは、ブレンドできないのでOFFにする
		if (state.clips.size() < 2)
		{
			blend.enabled = false;
			return;
		}

		ImGui::Checkbox("Blend", &blend.enabled);
		if (!blend.enabled) return;

		//ブレンド相手の番号が範囲外なら、先頭のクリップにする
		if (!state.IsValidClipIndex(blend.targetClipIndex))
		{
			blend.targetClipIndex = 0;
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.0f);
		if (ImGui::BeginCombo("##BlendTarget", state.clips[blend.targetClipIndex].name_.c_str()))
		{
			for (int i = 0; i < static_cast<int>(state.clips.size()); ++i)
			{
				const bool isSelected = (blend.targetClipIndex == i);
				const std::string label = state.clips[i].name_ + "##blend" + std::to_string(i);
				if (ImGui::Selectable(label.c_str(), isSelected))
				{
					blend.targetClipIndex = i;
				}
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(150.0f);
		ImGui::SliderFloat("Weight", &blend.weight, 0.0f, 1.0f, "%.2f");
	}

	/// <summary>
	/// ←→：前後のキーへ移動　Shift+←→：1フレーム移動(どちらも端でループ)
	/// </summary>
	void HandleKeyNavigation(PlaybackState& playback, const std::vector<float>& keyTimes, float duration)
	{
		//文字入力中は矢印キーをカーソル移動に使うので反応しない
		if (ImGui::GetIO().WantTextInput) return;

		int direction = 0;
		if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) direction = +1;
		if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow))  direction = -1;
		if (direction == 0) return;

		playback.isPlaying = false;
		playback.time = ImGui::GetIO().KeyShift
			? StepFrame(playback.time, duration, direction)
			: FindAdjacentKeyTime(keyTimes, playback.time, direction);
	}

	/// <summary>
	/// シークバーと、その上のキーマーカー(クリックでその時刻へ移動)
	/// </summary>
	void DrawSeekBar(PlaybackState& playback, const std::vector<float>& keyTimes, float duration)
	{
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::SliderFloat("##TimeSeek", &playback.time, 0.0f, (duration > 0.0f ? duration : 1.0f), "%.3f s");

		//シークバーの画面上の範囲
		const ImVec2 barMin = ImGui::GetItemRectMin();
		const ImVec2 barMax = ImGui::GetItemRectMax();
		const float barWidth = barMax.x - barMin.x;
		if (duration <= 0.0f || barWidth <= 0.0f) return;

		//シークバーより手前に描くため、最前面の描画リストを使う
		ImDrawList* drawList = ImGui::GetForegroundDrawList();
		const float centerY = (barMin.y + barMax.y) * 0.5f;
		const ImVec2 mouse = ImGui::GetMousePos();
		const bool clicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);

		for (float keyTime : keyTimes)
		{
			const float posX = barMin.x + std::clamp(keyTime / duration, 0.0f, 1.0f) * barWidth;

			//マーカーがクリックされたらその時刻へ移動
			if (clicked && std::abs(mouse.x - posX) <= kMarkerHitRadius && std::abs(mouse.y - centerY) <= kMarkerHitRadius)
			{
				playback.time = keyTime;
				playback.isPlaying = false;
				ImGui::ClearActiveID();	//下のシークバーがドラッグ状態にならないようにする
			}

			//再生位置にあるキーは、選択中として白く大きく描く
			const bool isCurrent = std::abs(keyTime - playback.time) < EditorConst::kKeyTimeEpsilon;
			const float r = isCurrent ? kCurrentMarkerRadius : kMarkerRadius;
			const ImVec2 diamond[4] = {
				ImVec2(posX, centerY - r),
				ImVec2(posX + r, centerY),
				ImVec2(posX, centerY + r),
				ImVec2(posX - r, centerY)
			};
			drawList->AddConvexPolyFilled(diamond, 4, isCurrent ? kCurrentMarkerColor : kMarkerColor);
			drawList->AddPolyline(diamond, 4, kMarkerOutlineColor, ImDrawFlags_Closed, 1.5f);
		}
	}

	/// <summary>
	/// 再生中なら時刻を進める
	/// </summary>
	void AdvanceTime(PlaybackState& playback, float deltaTime, float duration)
	{
		if (!playback.isPlaying || duration <= 0.0f) return;

		playback.time += deltaTime * playback.speed;

		if (playback.isLoop)
		{
			//範囲を出たら反対側へ回り込む(逆再生にも対応)
			if (playback.time > duration) playback.time = std::fmod(playback.time, duration);
			if (playback.time < 0.0f)     playback.time += duration;
		}
		else
		{
			//端に着いたら止める
			if (playback.time >= duration)
			{
				playback.time = duration;
				playback.isPlaying = false;
			}
			else if (playback.time <= 0.0f)
			{
				playback.time = 0.0f;
				playback.isPlaying = false;
			}
		}
	}
}

void TimeLineView::Draw(EditorState& state, float deltaTime)
{
	ImGui::SetNextWindowSize(kWindowSize, ImGuiCond_FirstUseEver);
	ImGui::Begin("Timeline");

	//クリップが無いときは操作方法を表示するだけ
	if (state.clips.empty())
	{
		ImGui::Text("Drag & Drop Base Model (.fbx)");
		ImGui::Text("Add Animation via File -> AddAnimation(.fbx)");
		ImGui::End();
		return;
	}

	//クリップの番号が範囲外なら先頭にする
	if (!state.IsValidClipIndex(state.currentClipIndex))
	{
		state.currentClipIndex = 0;
	}

	DrawClipSelector(state);
	DrawPlaybackControls(state);
	DrawBlendControls(state);

	//ここから下は、選択中のクリップについての表示と操作
	const AnimationClip& clip = state.clips[state.currentClipIndex];
	const float duration = clip.duration_;
	const std::vector<float> keyTimes = CollectKeyTimes(clip, state.selectedBoneIndex);

	HandleKeyNavigation(state.playback, keyTimes, duration);
	DrawSeekBar(state.playback, keyTimes, duration);
	AdvanceTime(state.playback, deltaTime, duration);

	ImGui::End();
}