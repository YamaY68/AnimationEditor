#pragma once
#include<string>
#include"../Animation/Skeleton.h"
#include"../Animation/AnimationClip.h"


class FbxExporter
{
public:
	bool Export(const std::string&filePath,const Skeleton& skelton, const AnimationClip& clip);
};

