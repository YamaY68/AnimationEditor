#include "Application.h"

/// @brief Windowsアプリケーションのエントリーポイント
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    Application app;
    if (!app.Init())
    {
        return -1;
    }

    app.Run();
    app.Release();

    return 0;
}