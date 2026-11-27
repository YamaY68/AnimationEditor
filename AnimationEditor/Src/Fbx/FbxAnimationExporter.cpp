#include "FbxAnimationExporter.h"
#include<fbxsdk.h>
#include<algorithm>
#include<cmath>
#include"../Common/EditorConstants.h"

using namespace DirectX;

namespace
{
	//FBXの1ボーン分のアニメーションカーブ(移動・回転・スケールのXYZ)
	struct BoneCurves
	{
		FbxNode* node = nullptr;
		FbxAnimCurve* translation[3] = {};
		FbxAnimCurve* rotation[3] = {};
		FbxAnimCurve* scaling[3] = {};
	};

	//Shift-JIS(ファイルダイアログが返すパス)を、UTF-8(FBX SDKに渡すパス)に変換する
	std::string AnsiToUtf8(const std::string& ansi)
	{
		char* utf8 = nullptr;
		FbxAnsiToUTF8(ansi.c_str(), utf8);
		std::string result = utf8 ? utf8 : "";
		FbxFree(utf8);
		return result;
	}

	//Transform(移動・回転・スケール)を、FBXのLcl Translation / Rotation / Scaling の値にする
	//(回転はXYZ順のオイラー角(度))
	void ToFbxTRS(const Transform& trs, FbxVector4& outT, FbxVector4& outR, FbxVector4& outS)
	{
		FbxAMatrix m;
		m.SetTQS(
			FbxVector4(trs.translation_.x, trs.translation_.y, trs.translation_.z),
			FbxQuaternion(trs.rotation_.x, trs.rotation_.y, trs.rotation_.z, trs.rotation_.w),
			FbxVector4(trs.scale_.x, trs.scale_.y, trs.scale_.z));
		outT = m.GetT();
		outR = m.GetR();
		outS = m.GetS();
	}

	/// <summary>
	/// スケルトンと同じ名前・親子関係で、ボーンだけのノードをシーンに作る
	/// (メッシュ・スキン・マテリアルは作らない。アニメーションだけのFBXにするため)
	/// </summary>
	/// <returns>ボーン番号ごとのノード</returns>
	std::vector<FbxNode*> CreateBoneNodes(FbxScene* scene, const Skeleton& skeleton)
	{
		std::vector<FbxNode*> nodes(skeleton.GetBoneCount(), nullptr);
		for (size_t b = 0; b < skeleton.GetBoneCount(); ++b)
		{
			const Bone& bone = skeleton.GetBone(b);
			FbxNode* node = FbxNode::Create(scene, bone.name_.c_str());

			//ボーンとして扱われるよう、スケルトンの属性を付ける(一番上だけRoot、ほかはLimbNode)
			FbxSkeleton* attribute = FbxSkeleton::Create(scene, bone.name_.c_str());
			attribute->SetSkeletonType(bone.parentIndex_ < 0 ? FbxSkeleton::eRoot : FbxSkeleton::eLimbNode);
			node->SetNodeAttribute(attribute);

			//アニメーションが無いときの姿勢は、バインドポーズにする
			//(新しく作ったノードはピボットや前回転が無く、回転順もXYZなので、ローカル姿勢をそのまま書ける)
			FbxVector4 t, r, sc;
			ToFbxTRS(bone.bindPoseLocal_, t, r, sc);
			node->LclTranslation.Set(FbxDouble3(t[0], t[1], t[2]));
			node->LclRotation.Set(FbxDouble3(r[0], r[1], r[2]));
			node->LclScaling.Set(FbxDouble3(sc[0], sc[1], sc[2]));

			//親の下につなぐ(ボーンは親が先に並んでいるので、親のノードはもう作ってある)
			const int parent = bone.parentIndex_;
			FbxNode* parentNode = (parent >= 0 && parent < static_cast<int>(b)) ? nodes[parent] : scene->GetRootNode();
			parentNode->AddChild(node);
			nodes[b] = node;
		}
		return nodes;
	}

	//AnimStackの名前に使えない文字を置き換える
	std::string ToStackName(const std::string& clipName)
	{
		std::string name = clipName.empty() ? "Take" : clipName;
		std::replace(name.begin(), name.end(), '/', '_');
		return name;
	}

	//キーを1つ打つ(線形補間)
	void AddKey(FbxAnimCurve* curve, const FbxTime& time, double value)
	{
		const int index = curve->KeyAdd(time);
		curve->KeySet(index, time, static_cast<float>(value), FbxAnimCurveDef::eInterpolationLinear);
	}

	/// <summary>
	/// クリップ1本を、一定間隔で全ボーンのキーにしてAnimStackを作る
	/// </summary>
	void BakeClip(FbxScene* scene, const Skeleton& skeleton, const std::vector<FbxNode*>& boneNodes,
		const AnimationClip& clip)
	{
		const std::string stackName = ToStackName(clip.name_);
		FbxAnimStack* stack = FbxAnimStack::Create(scene, stackName.c_str());
		FbxAnimLayer* layer = FbxAnimLayer::Create(scene, "Base Layer");
		stack->AddMember(layer);

		//ボーンごとにカーブを作る
		std::vector<BoneCurves> curves;
		curves.reserve(boneNodes.size());
		for (FbxNode* node : boneNodes)
		{
			BoneCurves c;
			c.node = node;
			if (node)
			{
				const char* axes[3] = { FBXSDK_CURVENODE_COMPONENT_X, FBXSDK_CURVENODE_COMPONENT_Y, FBXSDK_CURVENODE_COMPONENT_Z };
				for (int a = 0; a < 3; ++a)
				{
					c.translation[a] = node->LclTranslation.GetCurve(layer, axes[a], true);
					c.rotation[a] = node->LclRotation.GetCurve(layer, axes[a], true);
					c.scaling[a] = node->LclScaling.GetCurve(layer, axes[a], true);
					c.translation[a]->KeyModifyBegin();
					c.rotation[a]->KeyModifyBegin();
					c.scaling[a]->KeyModifyBegin();
				}
			}
			curves.push_back(c);
		}

		//一定間隔で姿勢を求めてキーを打つ(最後は端数があっても終端ちょうどにする。読み込みと同じ)
		const float duration = clip.duration_;
		const int frameCount = static_cast<int>(duration * EditorConst::kSampleFrameRate);
		Pose pose;
		for (int f = 0; f <= frameCount; ++f)
		{
			const float t = (f == frameCount) ? duration : (f * EditorConst::kFrameDuration);
			FbxTime time;
			time.SetSecondDouble(t);

			//キーの無いボーンはバインドポーズのまま
			pose = skeleton.CreateBindPose();
			clip.Sample(t, skeleton, pose);

			for (size_t b = 0; b < curves.size(); ++b)
			{
				const BoneCurves& c = curves[b];
				if (!c.node) continue;

				FbxVector4 tr, rot, sc;
				ToFbxTRS(pose[b], tr, rot, sc);
				for (int a = 0; a < 3; ++a)
				{
					AddKey(c.translation[a], time, tr[a]);
					AddKey(c.rotation[a], time, rot[a]);
					AddKey(c.scaling[a], time, sc[a]);
				}
			}
		}

		for (BoneCurves& c : curves)
		{
			if (!c.node) continue;
			for (int a = 0; a < 3; ++a)
			{
				c.translation[a]->KeyModifyEnd();
				c.rotation[a]->KeyModifyEnd();
				c.scaling[a]->KeyModifyEnd();
			}
			//オイラー角が±180度をまたいで飛ばないようにつなげる
			//(飛んだままだと、キーとキーの間で逆向きにぐるっと回ってしまう)
			FbxAnimCurveFilterUnroll unroll;
			unroll.Apply(c.rotation, 3);
		}

		//AnimStackの長さ
		FbxTime start;
		FbxTime stop;
		start.SetSecondDouble(0.0);
		stop.SetSecondDouble(duration);
		stack->LocalStart = start;
		stack->LocalStop = stop;
		stack->ReferenceStart = start;
		stack->ReferenceStop = stop;

		//読み込む側のツールが長さを見つけられるよう、TakeInfoにも書いておく
		FbxTakeInfo takeInfo;
		takeInfo.mName = stackName.c_str();
		takeInfo.mLocalTimeSpan.Set(start, stop);
		takeInfo.mReferenceTimeSpan.Set(start, stop);
		scene->SetTakeInfo(takeInfo);
	}
}

bool FbxAnimationExporter::Export(const std::string& modelPath, const std::string& outputPath,
	const Skeleton& skeleton, const std::vector<const AnimationClip*>& clips, std::string& outError)
{
	if (clips.empty())
	{
		outError = "書き出すクリップがありません";
		return false;
	}
	if (skeleton.GetBoneCount() == 0)
	{
		outError = "ボーンがありません";
		return false;
	}

	FbxManager* manager = FbxManager::Create();
	if (!manager)
	{
		outError = "FBX SDKを初期化できませんでした";
		return false;
	}
	FbxIOSettings* ioSettings = FbxIOSettings::Create(manager, IOSROOT);
	manager->SetIOSettings(ioSettings);

	bool succeeded = false;
	FbxScene* sourceScene = nullptr;
	FbxScene* scene = nullptr;
	do
	{
		//---------- モデルの元のFBXから、座標系と単位を取り出す ----------
		//(ボーンの値は元のFBXの単位(cm・mなど)のままなので、合わせないと大きさや向きが変わる)
		FbxImporter* importer = FbxImporter::Create(manager, "");
		if (!importer->Initialize(modelPath.c_str(), -1, ioSettings))
		{
			importer->Destroy();
			outError = "モデルのFBXを開けませんでした: " + modelPath;
			break;
		}
		sourceScene = FbxScene::Create(manager, "SourceScene");
		const bool imported = importer->Import(sourceScene);
		importer->Destroy();
		if (!imported)
		{
			outError = "モデルのFBXを読み込めませんでした: " + modelPath;
			break;
		}

		//---------- ボーンだけの新しいシーンを作る ----------
		scene = FbxScene::Create(manager, "ExportScene");
		FbxGlobalSettings& settings = scene->GetGlobalSettings();
		settings.SetAxisSystem(sourceScene->GetGlobalSettings().GetAxisSystem());
		settings.SetSystemUnit(sourceScene->GetGlobalSettings().GetSystemUnit());
		settings.SetTimeMode(FbxTime::ConvertFrameRateToTimeMode(EditorConst::kSampleFrameRate));

		const std::vector<FbxNode*> boneNodes = CreateBoneNodes(scene, skeleton);

		//---------- クリップをAnimStackにする ----------
		for (const AnimationClip* clip : clips)
		{
			if (clip) BakeClip(scene, skeleton, boneNodes, *clip);
		}
		//最初のクリップを、開いたときに選ばれているAnimStackにする
		if (FbxAnimStack* first = scene->GetSrcObject<FbxAnimStack>(0))
		{
			scene->SetCurrentAnimationStack(first);
			settings.SetTimelineDefaultTimeSpan(first->GetLocalTimeSpan());
		}

		//---------- 書き出す ----------
		ioSettings->SetBoolProp(EXP_FBX_ANIMATION, true);

		FbxExporter* exporter = FbxExporter::Create(manager, "");
		const int format = manager->GetIOPluginRegistry()->GetNativeWriterFormat();	//バイナリのFBX
		if (!exporter->Initialize(AnsiToUtf8(outputPath).c_str(), format, ioSettings))
		{
			outError = std::string("保存先を開けませんでした: ") + exporter->GetStatus().GetErrorString();
			exporter->Destroy();
			break;
		}
		succeeded = exporter->Export(scene);
		if (!succeeded)
		{
			outError = std::string("書き出しに失敗しました: ") + exporter->GetStatus().GetErrorString();
		}
		exporter->Destroy();
	} while (false);

	if (scene) scene->Destroy();
	if (sourceScene) sourceScene->Destroy();
	manager->Destroy();	//IOSettingsもここで一緒に消える
	return succeeded;
}