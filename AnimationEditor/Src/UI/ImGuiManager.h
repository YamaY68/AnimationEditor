#pragma once
#include <Windows.h>

struct ImFont;

class ImGuiManager
{
public:
	ImGuiManager(void) = default;
	~ImGuiManager(void) { Finalize(); }

	bool Initialize(HWND hWnd);

	void Finalize(void);

	void BeginFrame(void);

	void EndFrame(void);

	bool WantCaptureMouse(void)const;

	bool WantCaptureKeyboard(void) const;

	//日本語のフォント(Windowsに日本語のフォントが無ければnullptr)
	ImFont* GetJapaneseFont(void)const { return japaneseFont_; }

private:
	//Windowsに入っている日本語のフォントを読み込む(普段のUIのフォントはそのまま)
	void LoadJapaneseFont(void);

	bool isInitialized_ = false;
	ImFont* japaneseFont_ = nullptr;	//日本語を表示するところだけで使うフォント
};

