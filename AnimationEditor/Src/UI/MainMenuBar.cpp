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

			ImGui::Separator();

			//編集したアニメーションを、ボーンとアニメーションだけのFBXに書き出す
			if (ImGui::MenuItem("Export FBX (Current Clip)"))
			{
				std::string path = SaveFbxFileDialog(hWnd);
				if (!path.empty())
				{
					result.action = MenuAction::ExportCurrentClip;
					result.filePath = path;
				}
			}
			if (ImGui::MenuItem("Export FBX (All Clips)"))
			{
				std::string path = SaveFbxFileDialog(hWnd);
				if (!path.empty())
				{
					result.action = MenuAction::ExportAllClips;
					result.filePath = path;
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
	return result;
}