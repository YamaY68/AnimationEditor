#pragma once
#include<Windows.h>
#include<vector>
#include<string>

class ImGuiManager
{
public:
    ImGuiManager() = default;
    ~ImGuiManager() { Finalize(); }

    // 初期化（DxLib_Init の直後に呼び出す）
    bool Initialize(HWND hWnd);

    // 終了処理（DxLib_End の直前に呼び出す）
    void Finalize();

    // 毎フレームの描画開始宣言
    void BeginFrame();

    // 毎フレームの描画実行（DxLibの描画後、ScreenFlipの直前に呼び出す）
    void EndFrame();

    // UI上にマウスが乗っているか（カメラ操作の誤爆防止用）
    bool WantCaptureMouse() const;

    // UIがキー入力を受け取っているか
    bool WantCaptureKeyboard() const;
    std::vector<std::string> PopDroppedFiles();
private:
	bool isInitialized_ = false;
};

