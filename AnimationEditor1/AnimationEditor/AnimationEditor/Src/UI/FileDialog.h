#pragma once
#include <Windows.h>
#include <commdlg.h>
#include <string>

inline std::string OpenFbxFileDialog(HWND hWnd)
{
    char fileName[MAX_PATH] = "";
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = "FBX Files (*.fbx)\0*.fbx\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameA(&ofn))
    {
        return std::string(fileName);
    }
    return "";
}