#include "ControlsHelp.h"
#include<imgui.h>

//日本語の文字列をUTF-8で書くためのマクロ(ImGuiはUTF-8の文字列しか表示できないため)
//(ソースはShift-JISなので、u8を付けてUTF-8に変換させる。C++20ではu8はchar8_tなのでcharに直す)
#define U8(text) reinterpret_cast<const char*>(u8##text)

namespace
{
	constexpr float kKeyColumnWidth = 190.0f;					//キーの列の幅
	constexpr float kWindowAlpha = 0.92f;						//背景の不透明度
	constexpr ImVec4 kSectionColor(1.0f, 0.85f, 0.3f, 1.0f);	//見出しの色
	constexpr ImVec4 kKeyColor(0.55f, 0.85f, 1.0f, 1.0f);		//キーの色

	//操作1つ分(キーと説明。説明は日本語と英語の両方を持ち、日本語のフォントが無いときは英語を出す)
	struct Control
	{
		const char* key;
		const char* japanese;
		const char* english;
	};

	//見出しと、その下の操作の一覧
	struct Section
	{
		const char* japanese;
		const char* english;
		const Control* controls;
		int count;
	};

	const Control kCamera[] = {
		{ "Right Drag",  U8("回転"),       "Orbit" },
		{ "Middle Drag", U8("平行移動"),   "Pan" },
		{ "Wheel",       U8("ズーム"),     "Zoom" },
	};
	const Control kSelect[] = {
		{ "Left Click",  U8("ボーンを選ぶ(何も無い所で選択を外す)"), "Select bone (empty space: deselect)" },
		{ "B",           U8("ボーンの表示 / 非表示"),               "Show / hide bones" },
	};
	const Control kGizmo[] = {
		{ "W",           U8("移動"),                               "Move" },
		{ "E",           U8("回転"),                               "Rotate" },
		{ "I",           U8("IK(手首・足首を選ぶ)"),               "IK (select a hand or foot)" },
		{ "X",           U8("ローカル / ワールドの軸"),            "Local / World axis" },
		{ "Right Click", U8("移動 <-> 回転の切り替え"),            "Switch Move <-> Rotate" },
		{ "",            U8("(IK中: 移動 -> 回転 -> ポール)"),     "(IK: Move -> Rotate -> Pole)" },
	};
	const Control kIK[] = {
		{ "P",           U8("選んだ手首・足首をピン留め / 解除"),          "Pin / unpin selected hand or foot" },
		{ "IK Pole",     U8("水色の球をドラッグ: ひざ・ひじの曲がる向き"),         "Drag green sphere: knee / elbow direction" },
	};
	const Control kKeys[] = {
		{ "K",                U8("選んだボーンにキーを打つ"),       "Key selected bone" },
		{ "Delete",           U8("今の時刻のキーを消す"),           "Delete key at current time" },
		{ "Left / Right",     U8("前 / 次のキーへ"),                "Previous / next key" },
		{ "Shift+Left/Right", U8("前 / 次のフレームへ"),            "Previous / next frame" },
		{ "Marker Click",     U8("そのキーへ移動(タイムライン)"),   "Jump to key (timeline)" },
		{ "Marker Drag",      U8("キーを移す(タイムライン)"),       "Move key (timeline)" },
		{ "Marker R-Click",   U8("キーを消す(タイムライン)"),       "Delete key (timeline)" },
	};
	const Control kEdit[] = {
		{ "Ctrl+Z",                U8("元に戻す"),   "Undo" },
		{ "Ctrl+Y / Ctrl+Shift+Z", U8("やり直す"),   "Redo" },
	};
	const Control kFile[] = {
		{ "Drop .fbx",  U8("モデルを読み込む"),                         "Load model" },
		{ "File menu",  U8("アニメーションの追加 / FBXの書き出し"),     "Add animation / Export FBX" },
	};
	const Control kHelp[] = {
		{ "Tab",        U8("この操作説明の表示 / 非表示"),   "Show / hide this help" },
	};

	const Section kSections[] = {
		{ U8("カメラ"),         "Camera",    kCamera, IM_ARRAYSIZE(kCamera) },
		{ U8("選択"),           "Selection", kSelect, IM_ARRAYSIZE(kSelect) },
		{ U8("ギズモ"),         "Gizmo",     kGizmo,  IM_ARRAYSIZE(kGizmo) },
		{ U8("IK"),             "IK",        kIK,     IM_ARRAYSIZE(kIK) },
		{ U8("キーフレーム"),   "Keyframes", kKeys,   IM_ARRAYSIZE(kKeys) },
		{ U8("編集"),           "Edit",      kEdit,   IM_ARRAYSIZE(kEdit) },
		{ U8("ファイル"),       "File",      kFile,   IM_ARRAYSIZE(kFile) },
		{ U8("ヘルプ"),         "Help",      kHelp,   IM_ARRAYSIZE(kHelp) },
	};
}

void ControlsHelp::Draw(bool& inOutOpen, ImFont* japaneseFont)
{
	if (!inOutOpen) return;

	//日本語のフォントがあれば日本語、無ければ英語で表示する
	const bool useJapanese = (japaneseFont != nullptr);

	//開いたときに画面の真ん中に出す
	const ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowBgAlpha(kWindowAlpha);

	const ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse;
	if (ImGui::Begin("Controls (Tab)", &inOutOpen, flags))
	{
		if (useJapanese) ImGui::PushFont(japaneseFont);

		for (const Section& section : kSections)
		{
			ImGui::TextColored(kSectionColor, "%s", useJapanese ? section.japanese : section.english);
			for (int i = 0; i < section.count; ++i)
			{
				const Control& control = section.controls[i];
				ImGui::TextColored(kKeyColor, "  %s", control.key);
				ImGui::SameLine(kKeyColumnWidth);
				ImGui::TextUnformatted(useJapanese ? control.japanese : control.english);
			}
			ImGui::Spacing();
		}

		if (useJapanese) ImGui::PopFont();
	}
	ImGui::End();
}