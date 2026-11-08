#pragma once
#include <vector>
#include <string>
#include<algorithm>
#include <unordered_map>
#include <DxLib.h>
#include "Skeleton.h"
#include "../Common/Transform.h"

class ModelImporter
{
public:
    // Skeletonの各ボーンインデックスに対応する DxLib のフレーム番号テーブルを構築
    static std::vector<int> CreateBoneToFrameMap(int modelHandle, const Skeleton& skeleton) {
        std::vector<int> boneToFrame(skeleton.GetBoneCount(), -1);
        if (modelHandle == -1) return boneToFrame;

        // モデル内のフレーム名をハッシュマップ化
        int frameNum = MV1GetFrameNum(modelHandle);
        std::unordered_map<std::string, int> frameNameMap;
        for (int i = 0; i < frameNum; ++i) {
            const char* name = MV1GetFrameName(modelHandle, i);
            if (name) {
                frameNameMap[name] = i;
            }
        }

        // Skeletonのボーン名と一致するフレーム番号を記録
        for (int i = 0; i < skeleton.GetBoneCount(); ++i) {
            const auto& boneName = skeleton.GetBone(i).name_;
            auto it = frameNameMap.find(boneName);
            if (it != frameNameMap.end()) {
                boneToFrame[i] = it->second;
            }
        }

        return boneToFrame;
    }

    // 自前でサンプリングしたローカル姿勢（TRS）をDxLibモデルの対応フレームに流し込む
    static void ApplyPoseToModelMapped(int modelHandle, const Pose& pose, const std::vector<int>& boneToFrame) {
        if (modelHandle == -1) return;

        using namespace DirectX;
        size_t count = min(pose.localTransforms_.size(), boneToFrame.size());

        for (size_t i = 0; i < count; ++i) {
            int frameIdx = boneToFrame[i];
            if (frameIdx == -1) continue;

            // ローカルTRSを行列に変換
            XMMATRIX mat = pose.localTransforms_[i].ToMatrix();
            XMFLOAT4X4 fMat;
            XMStoreFloat4x4(&fMat, mat);

            // DxLib の MATRIX に転送
            MATRIX dxMat;
            for (int r = 0; r < 4; ++r) {
                for (int c = 0; c < 4; ++c) {
                    dxMat.m[r][c] = fMat.m[r][c];
                }
            }

            // 該当ボーンのローカル行列を上書き
            MV1SetFrameUserLocalMatrix(modelHandle, frameIdx, dxMat);
        }
    }
};