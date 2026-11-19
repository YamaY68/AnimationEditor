#include "MainMenuBar.h"
#include<imgui.h>
#include"FileDialogHelper.h"

MenuResult MainMenuBar::Draw(HWND hWnd)
{
	MenuResult result;

	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			//読み込み中のモデルに、アニメーションFBXを追加する
			if (ImGui::MenuItem("AddAnimation(.fbx)"))
			{
				std::string path = OpenFbxFileDialog(hWnd);
				if (!path.empty())
				{
					result.action = MenuAction::AddAnimation;
					result.filePath = path;
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	return result;
}