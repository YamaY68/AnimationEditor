#pragma once
#include <string>
#include <vector>
#include <fbxsdk.h>
#include "../Animation/Skeleton.h"
#include "../Animation/AnimationClip.h"
#include "FbxMeshModel.h" +
#if defined(_DEBUG)
#pragma comment(lib, "libfbxsdk-mt.lib")
#pragma comment(lib, "libxml2-mt.lib")
#pragma comment(lib, "zlib-mt.lib")
#else
#pragma comment(lib, "libfbxsdk-mt.lib")
#pragma comment(lib, "libxml2-mt.lib")
#pragma comment(lib, "zlib-mt.lib")
#endif

// Windows ïWèÄÉâÉCÉuÉâÉä
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "bcrypt.lib")

namespace Editor {


    class FbxImporter {
    public:
        FbxImporter(void);
        ~FbxImporter(void);

        bool Init(void);
        void Release(void);

        bool LoadSkeleton(const std::string& filePath, Skeleton& outSkeleton);

        bool LoadAnimation(const std::string& filePath, const Skeleton& skeleton, AnimationClip& outClip);
    bool LoadMeshWithSkinning(const std::string& filePath, Skeleton& skeleton, FbxMeshModel& outModel);    private:
        void ProcessSkeletonHierarchy(FbxNode* node, int parentIndex, Skeleton& outSkeleton);
        Transform ExtractLocalTransform(FbxNode* node);

        void ExtractNodeAnimation(FbxNode* node, int boneIndex, FbxAnimLayer* animLayer,
            float duration, float frameRate, BoneTrack& outTrack);

        FbxManager* fbxManager_ = nullptr;
        FbxIOSettings* ioSettings_ = nullptr;
    };


}