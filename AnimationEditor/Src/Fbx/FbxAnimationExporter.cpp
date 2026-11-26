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

	//シーンに入っているアニメーションを全部消す
	//(カーブ → カーブノード → レイヤー → AnimStack の順に消す)
	template<class T>
	void DestroyAllOf(FbxScene* scene)
	{
		for (int i = scene->GetSrcObjectCount<T>() - 1; i >= 0; --i)
		{
			scene->GetSrcObject<T>(i)->Destroy();
		}
	}
	void RemoveAllAnimations(FbxScene* scene)
	{
		DestroyAllOf<FbxAnimCurve>(scene);
		DestroyAllOf<FbxAnimCurveNode>(scene);
		DestroyAllOf<FbxAnimLayer>(scene);
		DestroyAllOf<FbxAnimStack>(scene);
	}

	//ノードのピボット・前回転・後回転などを無くし、ローカル行列 = 移動×回転×スケール だけにする
	//エディタはボーンの姿勢を「ローカル行列を分解した値」で持っているので、
	//こうしておくと、その値をそのままLcl Translation / Rotation / Scalingに書ける
	//(スキンのバインドポーズはクラスタにワールド行列で記録されているので、ここを変えても崩れない)
	void ResetPivots(FbxNode* node, const Transform& bindPose)
	{
		const FbxVector4 zero(0.0, 0.0, 0.0);
		node->SetRotationOffset(FbxNode::eSourcePivot, zero);
		node->SetRotationPivot(FbxNode::eSourcePivot, zero);
		node->SetPreRotation(FbxNode::eSourcePivot, zero);
		node->SetPostRotation(FbxNode::eSourcePivot, zero);
		node->SetScalingOffset(FbxNode::eSourcePivot, zero);
		node->SetScalingPivot(FbxNode::eSourcePivot, zero);
		node->SetRotationOrder(FbxNode::eSourcePivot, eEulerXYZ);

		//アニメーションが無いときの姿勢は、バインドポーズにする
		FbxVector4 t, r, s;
		ToFbxTRS(bindPose, t, r, s);
		node->LclTranslation.Set(FbxDouble3(t[0], t[1], t[2]));
		node->LclRotation.Set(FbxDouble3(r[0], r[1], r[2]));
		node->LclScaling.Set(FbxDouble3(s[0], s[1], s[2]));
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

	//書き出し用に、FBX SDKを別に用意する(読み込み用のシーンは三角形化などで書き換わっているため)
	FbxManager* manager = FbxManager::Create();
	if (!manager)
	{
		outError = "FBX SDKを初期化できませんでした";
		return false;
	}
	FbxIOSettings* ioSettings = FbxIOSettings::Create(manager, IOSROOT);
	manager->SetIOSettings(ioSettings);

	bool succeeded = false;
	FbxScene* scene = nullptr;
	do
	{
		//---------- モデルの元のFBXを開き直す ----------
		FbxImporter* importer = FbxImporter::Create(manager, "");
		if (!importer->Initialize(modelPath.c_str(), -1, ioSettings))
		{
			importer->Destroy();
			outError = "モデルのFBXを開けませんでした: " + modelPath;
			break;
		}
		scene = FbxScene::Create(manager, "ExportScene");
		const bool imported = importer->Import(scene);
		importer->Destroy();
		if (!imported)
		{
			outError = "モデルのFBXを読み込めませんでした: " + modelPath;
			break;
		}

		//---------- 元のアニメーションを消し、ボーンのノードを整える ----------
		RemoveAllAnimations(scene);

		std::vector<FbxNode*> boneNodes(skeleton.GetBoneCount(), nullptr);
		int foundCount = 0;
		for (size_t b = 0; b < skeleton.GetBoneCount(); ++b)
		{
			FbxNode* node = scene->FindNodeByName(skeleton.GetBone(b).name_.c_str());
			if (!node) continue;
			ResetPivots(node, skeleton.GetBone(b).bindPoseLocal_);
			boneNodes[b] = node;
			++foundCount;
		}
		if (foundCount == 0)
		{
			outError = "モデルのFBXにボーンが見つかりませんでした";
			break;
		}

		//---------- クリップをAnimStackにする ----------
		scene->GetGlobalSettings().SetTimeMode(FbxTime::ConvertFrameRateToTimeMode(EditorConst::kSampleFrameRate));
		for (const AnimationClip* clip : clips)
		{
			if (clip) BakeClip(scene, skeleton, boneNodes, *clip);
		}
		//最初のクリップを、開いたときに選ばれているAnimStackにする
		if (FbxAnimStack* first = scene->GetSrcObject<FbxAnimStack>(0))
		{
			scene->SetCurrentAnimationStack(first);
			scene->GetGlobalSettings().SetTimelineDefaultTimeSpan(first->GetLocalTimeSpan());
		}

		//---------- 書き出す ----------
		//テクスチャもFBXの中に埋め込む(元のFBXが埋め込みだった場合、そのままでは見つからなくなるため)
		ioSettings->SetBoolProp(EXP_FBX_EMBEDDED, true);
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
	manager->Destroy();	//IOSettingsもここで一緒に消える
	return succeeded;
}