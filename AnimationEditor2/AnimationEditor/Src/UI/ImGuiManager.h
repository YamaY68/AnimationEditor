#pragma once
#include <Windows.h>
#include<vector>
#include<string>

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

	std::vector<std::string>PopDroppedFiles(void);

private:
	bool isInitialized_ = false;
};

