#pragma once
#include <Windows.h>


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

private:
	bool isInitialized_ = false;
};

