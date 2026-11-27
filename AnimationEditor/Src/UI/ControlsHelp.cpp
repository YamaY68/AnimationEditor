#include "ControlsHelp.h"
#include<imgui.h>

namespace
{
	constexpr float kKeyColumnWidth = 190.0f;					//キーの列の幅
	constexpr float kWindowAlpha = 0.92f;						//背景の不透明度
	constexpr ImVec4 kSectionColor(1.0f, 0.85f, 0.3f, 1.0f);	//見出しの色
	constexpr ImVec4 kKeyColor(0.55f, 0.85f, 1.0f, 1.0f);		//キーの色

	//操作1つ分(キーと説明)
	struct Control
	{
		const char* key;
		const char* action;
	};

	//見出しと、その下の操作の一覧
	struct Section
	{
		const char* title;
		const Control* controls;
		int count;
	};

	const Control kCamera[] = {
		{ "Right Drag",  "Orbit" },
		{ "Middle Drag", "Pan" },
		{ "Wheel",       "Zoom" },
	};
	const Control kSelect[] = {
		{ "Left Click",  "Select bone (empty space: deselect)" },
		{ "B",           "Show / hide bones" },
	};
	const Control kGizmo[] = {
		{ "W",           "Move" },
		{ "E",           "Rotate" },
		{ "I",           "IK (select a hand or foot)" },
		{ "X",           "Local / World axis" },
		{ "Right Click", "Switch Move <-> Rotate" },
		{ "",            "(IK: Move -> Rotate -> Pole)" },
	};
	const Control kIK[] = {
		{ "P",           "Pin / unpin selected hand or foot" },
		{ "IK Pole",     "Drag green sphere: knee / elbow direction" },
	};
	const Control kKeys[] = {
		{ "K",              "Key selected bone" },
		{ "Delete",         "Delete key at current time" },
		{ "Left / Right",   "Previous / next key" },
		{ "Shift+Left/Right","Previous / next frame" },
		{ "Marker Click",   "Jump to key (timeline)" },
		{ "Marker Drag",    "Move key (timeline)" },
		{ "Marker R-Click", "Delete key (timeline)" },
	};
	const Control kEdit[] = {
		{ "Ctrl+Z",           "Undo" },
		{ "Ctrl+Y / Ctrl+Shift+Z", "Redo" },
	};
	const Control kFile[] = {
		{ "Drop .fbx",       "Load model" },
		{ "File menu",       "Add animation / Export FBX" },
	};
	const Control kHelp[] = {
		{ "Tab",             "Show / hide this help" },
	};

	const Section kSections[] = {
		{ "Camera",    kCamera, IM_ARRAYSIZE(kCamera) },
		{ "Selection", kSelect, IM_ARRAYSIZE(kSelect) },
		{ "Gizmo",     kGizmo,  IM_ARRAYSIZE(kGizmo) },
		{ "IK",        kIK,     IM_ARRAYSIZE(kIK) },
		{ "Keyframes", kKeys,   IM_ARRAYSIZE(kKeys) },
		{ "Edit",      kEdit,   IM_ARRAYSIZE(kEdit) },
		{ "File",      kFile,   IM_ARRAYSIZE(kFile) },
		{ "Help",      kHelp,   IM_ARRAYSIZE(kHelp) },
	};
}

void ControlsHelp::Draw(bool& inOutOpen)
{
	if (!inOutOpen) return;

	//開いたときに画面の真ん中に出す
	const ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowBgAlpha(kWindowAlpha);

	const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse;
	if (ImGui::Begin("Controls (Tab)", &inOutOpen, flags))
	{
		for (const Section& section : kSections)
		{
			ImGui::TextColored(kSectionColor, "%s", section.title);
			for (int i = 0; i < section.count; ++i)
			{
				ImGui::TextColored(kKeyColor, "  %s", section.controls[i].key);
				ImGui::SameLine(kKeyColumnWidth);
				ImGui::TextUnformatted(section.controls[i].action);
			}
			ImGui::Spacing();
		}
	}
	ImGui::End();
}