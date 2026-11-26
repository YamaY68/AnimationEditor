#include "BezierCurveWidget.h"
#include<imgui.h>
#include<algorithm>

namespace
{
	constexpr float kHandleRadius = 5.0f;		//制御点の丸の半径
	constexpr float kGrabRadius = 10.0f;		//制御点をつかめる距離(ピクセル)
	constexpr int kCurveSegments = 40;			//曲線を描くときの分割数
	constexpr int kPresetsPerRow = 3;			//プリセットのボタンを1行に並べる数

	constexpr ImU32 kBackgroundColor = IM_COL32(30, 30, 30, 255);
	constexpr ImU32 kGuideColor = IM_COL32(90, 90, 90, 255);		//0と1の線
	constexpr ImU32 kHandleColor = IM_COL32(255, 180, 60, 255);	//制御点
	constexpr ImU32 kCurveColor = IM_COL32(255, 255, 255, 255);	//曲線

	float DistSq(const ImVec2& a, const ImVec2& b)
	{
		return (a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y);
	}
}

bool BezierCurveWidget::Draw(const char* id, BezierHandles& h, const Options& options, bool& outEditStarted)
{
	outEditStarted = false;
	bool changed = false;
	ImGui::PushID(id);

	const float vMin = options.valueMin;
	const float vMax = options.valueMax;

	//---------- グラフ ----------
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	const float width = ImGui::GetContentRegionAvail().x;
	const ImVec2 size(width, options.height);

	//曲線の座標(x:0～1, y:縦軸の範囲) ⇔ 画面の座標
	auto toScreen = [&](float x, float y)
		{
			const float ny = (y - vMin) / (vMax - vMin);
			return ImVec2(origin.x + x * size.x, origin.y + (1.0f - ny) * size.y);
		};
	auto toCurve = [&](const ImVec2& p, float& outX, float& outY)
		{
			outX = (p.x - origin.x) / size.x;
			outY = vMin + (1.0f - (p.y - origin.y) / size.y) * (vMax - vMin);
		};

	//背景と、0と1の線
	drawList->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), kBackgroundColor);
	drawList->AddLine(toScreen(0.0f, 0.0f), toScreen(1.0f, 0.0f), kGuideColor);
	drawList->AddLine(toScreen(0.0f, 1.0f), toScreen(1.0f, 1.0f), kGuideColor);

	//制御点への線
	const ImVec2 p0 = toScreen(0.0f, 0.0f);
	const ImVec2 p1 = toScreen(h.x1, h.y1);
	const ImVec2 p2 = toScreen(h.x2, h.y2);
	const ImVec2 p3 = toScreen(1.0f, 1.0f);
	drawList->AddLine(p0, p1, kHandleColor);
	drawList->AddLine(p3, p2, kHandleColor);

	//曲線(x方向に等間隔で値を求めて結ぶ)
	ImVec2 prev = p0;
	for (int i = 1; i <= kCurveSegments; ++i)
	{
		const float x = static_cast<float>(i) / kCurveSegments;
		const ImVec2 cur = toScreen(x, Easing::EvaluateBezier(x, h));
		drawList->AddLine(prev, cur, kCurveColor, 2.0f);
		prev = cur;
	}
	drawList->AddCircleFilled(p1, kHandleRadius, kHandleColor);
	drawList->AddCircleFilled(p2, kHandleRadius, kHandleColor);

	//マウスの操作を受け取る透明なボタン
	ImGui::InvisibleButton("##Graph", size);
	const ImVec2 mouse = ImGui::GetIO().MousePos;

	//ドラッグ中の制御点(0:なし 1:1つ目 2:2つ目)は、ImGuiのウィジェットごとの保存領域に持つ
	//(グラフを複数出しても混ざらないように)
	ImGuiStorage* storage = ImGui::GetStateStorage();
	const ImGuiID dragKey = ImGui::GetID("##DragHandle");
	int dragHandle = storage->GetInt(dragKey, 0);

	//クリックした場所に近い制御点をつかむ
	if (ImGui::IsItemActivated())
	{
		const float d1 = DistSq(mouse, p1);
		const float d2 = DistSq(mouse, p2);
		const float grab = kGrabRadius * kGrabRadius;
		dragHandle = 0;
		if (d1 <= grab || d2 <= grab) dragHandle = (d1 <= d2) ? 1 : 2;
		if (dragHandle != 0) outEditStarted = true;
	}
	if (ImGui::IsItemActive() && dragHandle != 0)
	{
		float x, y;
		toCurve(mouse, x, y);
		x = std::clamp(x, 0.0f, 1.0f);
		y = std::clamp(y, vMin, vMax);
		if (dragHandle == 1) { h.x1 = x; h.y1 = y; }
		else { h.x2 = x; h.y2 = y; }
		changed = true;
	}
	if (!ImGui::IsItemActive()) dragHandle = 0;
	storage->SetInt(dragKey, dragHandle);

	//---------- プリセット ----------
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const float buttonWidth = (width - spacing * (kPresetsPerRow - 1)) / kPresetsPerRow;
	for (int i = 0; i < options.presetCount; ++i)
	{
		if (i % kPresetsPerRow != 0) ImGui::SameLine();
		if (ImGui::Button(options.presets[i].name, ImVec2(buttonWidth, 0.0f)))
		{
			outEditStarted = true;
			h = options.presets[i].handles;
			changed = true;
		}
	}

	//---------- 数値 ----------
	float values[4] = { h.x1, h.y1, h.x2, h.y2 };
	ImGui::SetNextItemWidth(-1.0f);
	if (ImGui::DragFloat4("##Values", values, 0.01f, vMin, vMax, "%.2f"))
	{
		h = { std::clamp(values[0], 0.0f, 1.0f), std::clamp(values[1], vMin, vMax),
			std::clamp(values[2], 0.0f, 1.0f), std::clamp(values[3], vMin, vMax) };
		changed = true;
	}
	if (ImGui::IsItemActivated()) outEditStarted = true;

	ImGui::PopID();
	return changed;
}