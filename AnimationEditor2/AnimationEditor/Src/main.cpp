#include<DxLib.h>
#include<cmath>
#include<vector>
#include<algorithm>
#include<filesystem>

#include"Camera/Camera.h"


void DrawGridAndAxes(float size, int divisions)
{
	float half = size * 0.5f;
	float step = size / divisions;
	unsigned int gridColor = GetColor(70, 70, 70);

	for (int i = 0; i <= divisions; ++i)
	{
		float pos = -half + i * step;
		DrawLine3D(VGet(pos, 0.0f, -half), VGet(pos, 0.0f, half), gridColor);
		DrawLine3D(VGet(-half, 0.0f, pos), VGet(half, 0.0f, pos), gridColor);
	}

	DrawLine3D(VGet(0.0f, 0.0f, 0.0f), VGet(10.0f, 0.0f, 0.0f), GetColor(255, 0, 0));
	DrawLine3D(VGet(0.0f, 0.0f, 0.0f), VGet(0.0f, 10.0f, 0.0f), GetColor(0, 255, 0));
	DrawLine3D(VGet(0.0f, 0.0f, 0.0f), VGet(0.0f, 0.0f, 10.0f), GetColor(0, 0, 255));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	ChangeWindowMode(true);
	SetGraphMode(1280, 720, 32);

	if (DxLib_Init() == -1)return -1;
	SetDrawScreen(DX_SCREEN_BACK);
	SetUseZBuffer3D(true);
	SetWriteZBuffer3D(true);
	SetCameraNearFar(1.0f, 3000.0f);


	Camera camera;
	camera.SetTarget(VGet(0.0f, 10.0f, 0.0f));
	camera.SetDistance(40.0f);


	while (ProcessMessage() == 0 && CheckHitKey(KEY_INPUT_ESCAPE) == 0)
	{
		ClearDrawScreen();

		camera.Update(true);
		camera.ApplyToDxLib();

		DrawGridAndAxes(200.0f, 20);

		ScreenFlip();
	}

	DxLib_End();
	return 0;
}