#pragma once
#include<vector>
#include"../Animation/AnimationClip.h"
#include"UndoStack.h"

/// <summary>
/// 再生の状態
/// </summary>
struct PlaybackState
{
	float time = 0.0f;		//現在の再生時刻(秒)
	bool isPlaying = true;	//再生中か
	bool isLoop = true;		//ループ再生するか
	float speed = 1.0f;		//再生速度の倍率(マイナスで逆再生)
};

/// <summary>
/// ブレンド再生の設定
/// </summary>
struct BlendState
{
	bool enabled = false;		//ブレンドするか
	int targetClipIndex = 0;	//ブレンド先のクリップ
	float weight = 0.0f;		//ブレンドの割合(0:今のクリップ ～ 1:ブレンド先)
};

/// <summary>
/// エディタの編集状態
/// タイムライン・インスペクターなどのUIは、これを受け取って読み書きする
/// (UIごとに状態を引数でバラバラに渡さずに済むようにまとめたもの)
/// </summary>
struct EditorState
{
	std::vector<AnimationClip> clips;	//読み込んだアニメーション
	int currentClipIndex = 0;			//再生・編集中のクリップ
	int selectedBoneIndex = -1;			//選択中のボーン(-1:未選択)
	bool autoKey = true;				//姿勢を変えたら自動でキーを打つか

	PlaybackState playback;
	BlendState blend;
	UndoStack undoStack;

	//クリップの番号が有効か
	bool IsValidClipIndex(int index)const
	{
		return index >= 0 && index < static_cast<int>(clips.size());
	}

	//再生・編集中のクリップ(無ければnullptr)
	AnimationClip* GetActiveClip(void)
	{
		return IsValidClipIndex(currentClipIndex) ? &clips[currentClipIndex] : nullptr;
	}
	const AnimationClip* GetActiveClip(void)const
	{
		return IsValidClipIndex(currentClipIndex) ? &clips[currentClipIndex] : nullptr;
	}

	//今のクリップを編集する「直前」に呼ぶ(Undoの履歴に積む)
	void RecordUndo(void)
	{
		if (const AnimationClip* clip = GetActiveClip())
		{
			undoStack.Record(currentClipIndex, *clip);
		}
	}

	//モデルを読み込み直したときに、モデルに紐づく状態をリセットする
	//(再生速度やオートキーなどの設定はそのまま残す)
	void ResetForNewModel(void)
	{
		clips.clear();
		currentClipIndex = 0;
		selectedBoneIndex = -1;
		playback.time = 0.0f;
		blend = BlendState{};
		undoStack.Clear();
	}
};