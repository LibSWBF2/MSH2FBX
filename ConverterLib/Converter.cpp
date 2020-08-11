#include "stdafx.h"
#include "Converter.h"

namespace ConverterLib
{
	LogCallback Converter::OnLogCallback = nullptr;

	Converter::Converter()
	{
		// pipe LibSWBF2 logs to our log
		Logger::SetLogfileLevel(ELogType::Warning);
		Logger::SetLogCallback(&ReceiveLogFromLib);
	}

	void Converter::ReceiveLogFromLib(const LoggerEntry* entry)
	{
		Log(entry->ToString().Buffer(), (ELogType)entry->m_Level);
	}

	Converter::Converter(const fs::path& fbxFileName) : Converter()
	{
		Start(fbxFileName);
	}

	Converter::~Converter()
	{
		Close();
	}

	void Converter::Log(const string& msg, ELogType type)
	{
		if (OnLogCallback)
		{
			OnLogCallback(msg.c_str(), (uint8_t)type);
		}
	}

	void Converter::SetLogCallback(const LogCallback Callback)
	{
		OnLogCallback = Callback;
	}

	void Converter::CheckHierarchy()
	{
		if (Scene == nullptr)
		{
			Log("Current FBX Scene is NULL!", ELogType::Error);
			return;
		}

		vector<FbxNode*> processedNodes;
		function<void(FbxNode*,uint32_t)> checkHierarchyRecursive = [&](FbxNode* node, uint32_t depth)
		{
			if (node == nullptr)
			{
				Log("Given FbxNode is NULL!", ELogType::Error);
				return;
			}

			if (depth > 1000)
			{
				Log("Max recursion depth of 1000 reached, abort! Parentship circle?", ELogType::Error);
				return;
			}

			if (bPrintHierachy)
			{
				string concat = " ";
				for (uint32_t i = 0; i < depth; ++i)
				{
					concat += " ";
				}
				Log(concat + node->GetName(), ELogType::Info);
			}

			int numChildren = node->GetChildCount();
			for (int i = 0; i < numChildren; ++i)
			{
				FbxNode* child = node->GetChild(i);
				if (child == nullptr)
				{
					Log("Child of '" + string(node->GetName()) + "' was NULL!", ELogType::Error);
					continue;
				}

				if (std::find(processedNodes.begin(), processedNodes.end(), child) != processedNodes.end())
				{
					Log("Parentship circle detected! Node '" + string(child->GetName()) + "' was already processed!", ELogType::Error);
					Log("Parent of '"+string(child->GetName())+"' is '"+ string(node->GetName()) +"'", ELogType::Info);
					continue;
				}

				FbxNode* parent = child->GetParent();
				if (parent != node)
				{
					if (parent == nullptr)
					{
						Log("Inconsistent hierarchy! Node '"+string(child->GetName())+"' is child of '" + string(node->GetName()) + "', but the child's parent is NULL!", ELogType::Error);
					}
					else
					{
						Log("Inconsistent hierarchy! Node '" + string(child->GetName()) + "' is child of '" + string(node->GetName()) + "', but the child's parent is '" + string(parent->GetName()) + "'!", ELogType::Error);
					}
				}

				processedNodes.emplace_back(child);
				checkHierarchyRecursive(child, depth + 1);
			}
		};

		checkHierarchyRecursive(Scene->GetRootNode(), 0);
	}

	FbxNode* Converter::FindNode(MODL* model)
	{
		if (model == nullptr)
		{
			Log("Given MODL pointer was NULL!", ELogType::Error);
			return nullptr;
		}

		// Get FbxNode of the model (child)
		auto it = MODLToFbxNode.find(model);
		if (it != MODLToFbxNode.end())
		{
			if (it->second != nullptr)
			{
				return it->second;
			}
			else
			{
				Log("MODL '" + string(model->m_Parent.m_Text.Buffer()) + "' has been mapped to NULL!", ELogType::Error);
				return nullptr;
			}
		}
		return nullptr;
	}

	FbxNode* Converter::FindNode(const CRCChecksum checksum)
	{
		// get respective Bone to animate from stored CRC checksum
		auto it = CRCToFbxNode.find(checksum);
		if (it != CRCToFbxNode.end())
		{
			if (!it->second)
			{
				Log("CRC '" + std::to_string(checksum) + "' has been mapped to null pointer!", ELogType::Warning);
				return nullptr;
			}
			return it->second;
		}
		return nullptr;
	}

	bool Converter::Start(const fs::path& fbxFilePath)
	{
		if (bRunning)
		{
			Log("Cannot start Converter since it's already started!", ELogType::Error);
			return false;
		}

		if (Scene != nullptr)
		{
			Log("Scene is not NULL!", ELogType::Error);
			return false;
		}

		if (Manager != nullptr)
		{
			Log("Manager is not NULL!", ELogType::Error);
			return false;
		}

		if (Mesh != nullptr)
		{
			Log("Still a MSH present!", ELogType::Error);
			return false;
		}

		if (FbxFilePath != "")
		{
			Log("Still a Fbx File Name present!", ELogType::Error);
		}

		MODLToFbxNode.clear();
		CRCToFbxNode.clear();
		FbxFilePath = fbxFilePath;

		// Overall FBX (memory) manager
		Manager = FbxManager::Create();

		if (fs::exists(BaseposeMSH))
		{
			Basepose = MSH::Create();
			Basepose->ReadFromFile(BaseposeMSH.u8string().c_str());
		}

		// Create FBX Scene
		Scene = FbxScene::Create(Manager, fbxFilePath.filename().u8string().c_str());
		Scene->GetGlobalSettings().SetSystemUnit(FbxSystemUnit::m);
		bRunning = true;
		return true;
	}

	bool Converter::AddMSH(const fs::path& mshFilePath)
	{
		if (Mesh != nullptr)
		{
			Log("Converting Mesh is not NULL!", ELogType::Error);
			return false;
		}

		if (!fs::exists(mshFilePath))
		{
			Log("Given MSH file '"+ mshFilePath.u8string() +"' does not exist!", ELogType::Error);
			return false;
		}

		Mesh = MSH::Create();
		Mesh->ReadFromFile(mshFilePath.u8string().c_str());
		MSHToFBXScene();

		MSH::Destroy(Mesh);
		Mesh = nullptr;

		return true;
	}

	bool Converter::AddMSH(MSH* msh)
	{
		if (Mesh != nullptr)
		{
			Log("Converting Mesh is not NULL!", ELogType::Error);
			return false;
		}

		if (msh == nullptr)
		{
			Log("Given MSH pointer is NULL!", ELogType::Error);
			return false;
		}

		Mesh = msh;
		MSHToFBXScene();
		Mesh = nullptr;
		return true;
	}

	bool Converter::SaveFBX()
	{
		if (Scene == nullptr)
		{
			Log("Scene is NULL!", ELogType::Error);
			return false;
		}

		if (Manager == nullptr)
		{
			Log("Manager is NULL!", ELogType::Error);
			return false;
		}

		if (Mesh != nullptr)
		{
			Log("There is still a MSH in progress!", ELogType::Error);
			return false;
		}

		if (FbxFilePath == "")
		{
			Log("No Fbx File Name present!", ELogType::Error);
		}

		bool success = true;

		// Export Scene to FBX
		FbxExporter* exporter = FbxExporter::Create(Manager, "");
		FbxIOSettings* settings = FbxIOSettings::Create(Manager, IOSROOT);
		settings->SetBoolProp(EXP_FBX_MATERIAL, true);
		settings->SetBoolProp(EXP_FBX_TEXTURE, true);
		settings->SetBoolProp(EXP_FBX_ANIMATION, true);
		settings->SetBoolProp(EXP_FBX_GLOBAL_SETTINGS, true);
		Manager->SetIOSettings(settings);

		if (bPrintHierachy)
		{
			Log("Hierarchy of '"+ FbxFilePath.u8string() +"':", ELogType::Info);
		}
		CheckHierarchy();

		if (exporter->Initialize(FbxFilePath.u8string().c_str(), -1, Manager->GetIOSettings()))
		{
			exporter->SetFileExportVersion(FBX_2019_00_COMPATIBLE);
			if (!exporter->Export(Scene, false))
			{
				Log("Exporting failed!\n" + string(exporter->GetStatus().GetErrorString()), ELogType::Error);
				success = false;
			}
		}
		else
		{
			Log("Initializing export failed!\n" + string(exporter->GetStatus().GetErrorString()), ELogType::Error);
			success = false;
		}

		// Free all
		exporter->Destroy();
		return success;
	}

	bool Converter::ClearFBXScene()
	{
		if (!bRunning)
		{
			Log("Cannot clear a not running Converter instance!", ELogType::Error);
			return false;
		}

		fs::path p = FbxFilePath;
		Close();
		Start(p);
		return true;
	}

	void Converter::Close()
	{
		if (!bRunning)
		{
			return;
		}
		if (Scene == nullptr)
		{
			Log("Cannot close Scene since its NULL!", ELogType::Error);
			return;
		}

		if (Manager == nullptr)
		{
			Log("Cannot close Scene since its NULL!", ELogType::Error);
			return;
		}

		if (Basepose != nullptr)
		{
			MSH::Destroy(Basepose);
		}

		// Free all
		Scene->Destroy();
		Scene = nullptr;

		Manager->Destroy();
		Manager = nullptr;

		Mesh = nullptr;
		FbxFilePath = "";
		bRunning = false;
	}

	void Converter::MSHToFBXScene()
	{
		FbxNode* rootNode = Scene->GetRootNode();
		map<MODL*, FbxCluster*> BoneToCluster;

		// Converting Models
		if ((ChunkFilter & EChunkFilter::Models) == 0)
		{
			// Since we have to loop through all MSH models multiple times (duh!)
			// lets just remember all processed models (according to filter)
			// so we don't have to re-filter in the other loops again
			vector<MODL*> processingModels;

			for (size_t i = 0; i < Mesh->m_MeshBlock.m_Models.Size(); ++i)
			{
				MODL& model = Mesh->m_MeshBlock.m_Models[i];
				EModelPurpose purpose = model.GetPurpose();

				// generate crc checksum from name (should match those in msh file)
				CRCChecksum crc = CRC::CalcLowerCRC(model.m_Name.m_Text.Buffer());

				// Do not process unwanted stuff
				// Do not process the same Bone more than once
				if ((purpose & ModelIgnoreFilter) != 0 || ((purpose & EModelPurpose::Skeleton) != 0 && FindNode(crc) != nullptr))
				{
					continue;
				}

				// Create Node to attach mesh to
				FbxNode* modelNode = FbxNode::Create(Manager, model.m_Name.m_Text.Buffer());
				rootNode->AddChild(modelNode);

				if ((purpose & EModelPurpose::Mesh) != 0)
				{
					if (bEmptyMeshes)
					{
						FbxMesh* mesh = FbxMesh::Create(Manager, model.m_Name.m_Text.Buffer());
						modelNode->AddNodeAttribute(mesh);
					}

					// Create and attach Mesh
					else if (!MODLToFBXMesh(model, Mesh->m_MeshBlock.m_MaterialList, modelNode))
					{
						Log("Failed to convert MSH Model to FBX Mesh. MODL No: " + std::to_string(i) + "  MTYP: " + std::to_string((int)model.m_ModelType.m_ModelType), ELogType::Warning);
						continue;
					}
				}
				else if ((purpose & EModelPurpose::Skeleton) != 0)
				{
					// Create FBXSkeleton (Bones)
					if (!MODLToFBXSkeleton(model, modelNode))
					{
						Log("Failed to convert MSH Model to FBX Skeleton. MODL No: " + std::to_string(i) + "  MTYP: " + std::to_string((int)model.m_ModelType.m_ModelType), ELogType::Warning);
						continue;
					}
				}
				else // everything else is just interpreted as a point with an empty mesh
				{
					FbxMesh* mesh = FbxMesh::Create(Manager, model.m_Name.m_Text.Buffer());
					modelNode->AddNodeAttribute(mesh);
				}

				processingModels.emplace_back(&model); 
				CRCToFbxNode[crc] = modelNode;
				MODLToFbxNode[&model] = modelNode;
			}

			// Applying Transforms and Parentships
			// Maybe doing something more efficient in the future?
			for (size_t i = 0; i < processingModels.size(); ++i)
			{
				MODL* model = processingModels[i];
				EModelPurpose purpose = model->GetPurpose();

				FbxNode* modelNode = FindNode(model);
				if (modelNode == nullptr)
				{
					Log("No FbxNode has been created for MODL '" + string(model->m_Parent.m_Text.Buffer()) + "' ! This should never happen!", ELogType::Error);
					continue;
				}

				// Change parentship (if any)
				if (model->m_Parent.m_Text != "")
				{
					// Get FBXNode of Parent
					FbxNode* parentNode = rootNode->FindChild(model->m_Parent.m_Text.Buffer());

					if (parentNode != nullptr)
					{
						//Log(parentNode->GetName() + string(" --> ") + modelNode->GetName(), ELogType::Info);
						rootNode->RemoveChild(modelNode);
						parentNode->AddChild(modelNode);
					}
					else
					{
						Log("Parent Node '" + string(model->m_Parent.m_Text.Buffer()) + "' not found!", ELogType::Warning);
					}
				}

				// Applying MODL Transform to FbxNode
				ApplyTransform(
					modelNode, 
					model->m_Transition.m_Translation, 
					model->m_Transition.m_Rotation,
					model->m_Transition.m_Scale
				);
			}

			// Apply Basepose to all Bones BEFORE applying Mesh Weights!
			if (Basepose != nullptr)
			{
				List<BoneFrames>& BoneFrames = Basepose->m_Animations.m_KeyFrames.m_BoneFrames;
				if (BoneFrames.Size() > 0)
				{
					// for every bone...
					for (size_t i = 0; i < BoneFrames.Size(); ++i)
					{
						FbxNode* boneNode = FindNode(BoneFrames[i].m_CRCchecksum);

						if (boneNode == nullptr)
						{
							Log("Could not find a Bone for CRC: " + std::to_string(BoneFrames[i].m_CRCchecksum), ELogType::Warning);
							continue;
						}

						if (BoneFrames[i].m_TranslationFrames.Size() == 0)
						{
							Log("Given Basepose file does not contain any Bone Translation Data!", ELogType::Warning);
						}
						else if (BoneFrames[i].m_RotationFrames.Size() == 0)
						{
							Log("Given Basepose file does not contain any Bone Rotation Data!", ELogType::Warning);
						}
						else
						{
							// A Basepose file contains an Animation with only one Frame
							// defining the Basepose. So ignore all possible other frames
							Vector3& boneTranslation = BoneFrames[i].m_TranslationFrames[0].m_Translation;
							Vector4& boneRotation = BoneFrames[i].m_RotationFrames[0].m_Rotation;
							FbxVector4 rot = QuaternionToEuler(boneRotation);

							ApplyTransform(
								boneNode,
								boneTranslation,
								boneRotation
							);

							// Ensure FBX Bindepose exists
							if (Bindpose == nullptr)
							{
								Bindpose = FbxPose::Create(Scene, "Bindpose");
								Bindpose->SetIsBindPose(true);
								Scene->AddPose(Bindpose);
							}
							Bindpose->Add(boneNode, boneNode->EvaluateGlobalTransform(), false, false);
						}
					}
				}
				else
				{
					Log("Given Basepose file does not contain any Frame Data!", ELogType::Warning);
				}
			}


			// Applying Weights to all Meshes
			// Execute this AFTER all Bones (MODLs) are converted to FbxNodes
			// and their Transforms have been applied respectively!
			for (size_t i = 0; i < processingModels.size(); ++i)
			{
				MODL* model = processingModels[i];
				EModelPurpose purpose = model->GetPurpose();
				
				FbxNode* modelNode = FindNode(model);
				if (modelNode == nullptr)
				{
					Log("No FbxNode has been created for MODL '" + string(model->m_Parent.m_Text.Buffer()) + "' ! This should never happen!", ELogType::Error);
					continue;
				}

				if ((ChunkFilter & EChunkFilter::Weights) == 0 && (purpose & EModelPurpose::Mesh) != 0)
				{
					FbxAMatrix& matrixMeshNode = modelNode->EvaluateGlobalTransform();
					size_t vertexOffset = 0;

					// Go through all Mesh Segments, grabbing Weight data
					for (size_t i = 0; i < model->m_Geometry.m_Segments.Size(); ++i)
					{
						SEGM& segment = model->m_Geometry.m_Segments[i];
						WGHTToFBXSkin(segment.m_WeightList, model->m_Geometry.m_Envelope, matrixMeshNode, vertexOffset, BoneToCluster);
						vertexOffset += segment.m_VertexList.m_Vertices.Size();
					}

					FbxSkin* skin = FbxSkin::Create(Scene, (string(model->m_Name.m_Text.Buffer()) + "_Skin").c_str());

					for (auto it = BoneToCluster.begin(); it != BoneToCluster.end(); it++)
					{
						skin->AddCluster(it->second);
					}

					FbxMesh* mesh = (FbxMesh*)modelNode->GetNodeAttribute();
					mesh->AddDeformer(skin);
				}
			}
		}

		// Converting Animations
		if ((ChunkFilter & EChunkFilter::Animations) == 0)
		{
			ANM2ToFBXAnimations(Mesh->m_Animations);
		}
	}

	FbxDouble3 Converter::ColorToFBXColor(const Color& color)
	{
		return FbxDouble3(color.m_Red, color.m_Green, color.m_Blue);
	}

	FbxDouble4 Converter::QuaternionToEuler(const Vector4& Quaternion)
	{
		FbxQuaternion quaternion;
		quaternion.Set
		(
			Quaternion.m_X,
			Quaternion.m_Y,
			Quaternion.m_Z,
			Quaternion.m_W
		);

		FbxAMatrix rotMatrix;
		rotMatrix.SetQOnly(quaternion);
		return rotMatrix.GetROnly();
	}

	void Converter::ApplyTransform(FbxNode* modelNode, const Vector3& Translation, const Vector4& Rotation)
	{
		modelNode->LclTranslation.Set
		(
			FbxDouble3
			(
				Translation.m_X,
				Translation.m_Y,
				Translation.m_Z
			)
		);
		modelNode->LclRotation.Set(QuaternionToEuler(Rotation));
	}

	void Converter::ApplyTransform(FbxNode* modelNode, const Vector3& Translation, const Vector4& Rotation, const Vector3& Scale)
	{
		ApplyTransform(modelNode, Translation, Rotation);
		modelNode->LclScaling.Set
		(
			FbxDouble3
			(
				Scale.m_X,
				Scale.m_Y,
				Scale.m_Z
			)
		);
	}
	
	void Converter::WGHTToFBXSkin(WGHT& weights, const ENVL& envelope, const FbxAMatrix& matrixMeshNode, const size_t vertexOffset, map<MODL*, FbxCluster*>& BoneToCluster)
	{
		if (Mesh == nullptr)
		{
			Log("Mesh (MSH) is NULL!", ELogType::Error);
			return;
		}

		// for each vertex...
		for (size_t i = 0; i < weights.m_Weights.Size(); ++i)
		{
			// each vertex consist of 4 weights (for 4 bones)
			for (int j = 0; j < weights.m_Weights[i].NUM_OF_WEIGHTS; ++j)
			{
				BoneWeight& weight = weights.m_Weights[i].m_BoneWeights[j];
				uint32_t& ei = weight.m_EnvelopeIndex;

				// Do not process if the weight is 0.0 anyway
				if (weight.m_WeightValue == 0.0f)
				{
					continue;
				}
				
				if (ei < (uint32_t)envelope.m_ModelIndices.Size())
				{
					if (envelope.m_ModelIndices[ei] < Mesh->m_MeshBlock.m_Models.Size())
					{
						// Here, we're looking at the Bone  of the Mesh file, not the Basepose file!
						// Since the Bones from the Mesh File wont have been processed if a Basepose file
						// has been specified, finding the corresponding FbxNode via MODL pointer
						// won't yield any results. So we have to find the Bone via CRC
						MODL& bone = Mesh->m_MeshBlock.m_Models[envelope.m_ModelIndices[ei]];

						CRCChecksum crc = CRC::CalcLowerCRC(bone.m_Name.m_Text.Buffer());
						FbxNode* BoneNode = FindNode(crc);

						if (BoneNode == nullptr)
						{
							Log("Could not find a Bone '" + string(bone.m_Name.m_Text.Buffer()) + "' for CRC: " + std::to_string(crc), ELogType::Warning);
							continue;
						}

						// In MSH the end point represents the Bone, while in FBX the start point represents the bone.
						// This leads to inconsistent application of weights. 
						// To prevent that, we have to map the weights onto the Nodes parent

						// Example layout: root_r_upperarm --> bone_r_upperarm --> bone_r_forearm --> eff_r_forearm
						// In MSH, weights for upperarm and forearm are applied to: bone_r_forearm and eff_r_forearm
						// But in FBX, we want to apply them to: bone_r_upperarm and bone_r_forearm
						BoneNode = BoneNode->GetParent();

						if (BoneNode != nullptr)
						{
							FbxCluster* cluster = nullptr;

							auto it2 = BoneToCluster.find(&bone);
							if (it2 != BoneToCluster.end())
							{
								cluster = it2->second;
							}
							else
							{
								cluster = FbxCluster::Create(Scene, (string(bone.m_Name.m_Text.Buffer()) + "_Cluster").c_str());
								cluster->SetLinkMode(FbxCluster::eTotalOne);
								cluster->SetLink(BoneNode);
								BoneToCluster[&bone] = cluster;
							}

							// TODO: do all 4 weights really add up to 1.0 in MSH? investigation needed!
							cluster->AddControlPointIndex((int)(i + vertexOffset), (double)weight.m_WeightValue);
							cluster->SetTransformMatrix(matrixMeshNode);

							// bone node transform matrix
							FbxAMatrix& matrixBoneNode = BoneNode->EvaluateGlobalTransform();
							cluster->SetTransformLinkMatrix(matrixBoneNode);
						}
						else
						{
							Log("MODL (Bone) '" + string(bone.m_Parent.m_Text.Buffer()) + "' has been mapped to NULL!", ELogType::Warning);
						}
					}
					else
					{
						Log("Model Index " + std::to_string(envelope.m_ModelIndices[ei]) + " is out of Range " + std::to_string(Mesh->m_MeshBlock.m_Models.Size()), ELogType::Warning);
					}
				}
				else
				{
					Log("Envelope Index " + std::to_string(weight.m_EnvelopeIndex) + " is out of Range " + std::to_string(envelope.m_ModelIndices.Size()), ELogType::Warning);
				}
			}
		}
	}

	void Converter::ANM2ToFBXAnimations(ANM2& animations)
	{
		if (animations.m_AnimationCycle.m_Animations.Size() == 0)
		{
			Log("No Animation Circle found!", ELogType::Warning);
			return;
		}

		// assuming only one animation per msh, this should be temporary
		Animation& anim = animations.m_AnimationCycle.m_Animations[0];
		string& animName = OverrideAnimName != "" ? OverrideAnimName : anim.m_AnimationName.Buffer();
		FbxAnimStack* animStack = FbxAnimStack::Create(Scene, animName.c_str());

		FbxTime start, end;
		start.SetSecondDouble(anim.m_FirstFrame / anim.m_FrameRate);
		end.SetSecondDouble(anim.m_LastFrame / anim.m_FrameRate);
		animStack->SetLocalTimeSpan(FbxTimeSpan(start, end));

		FbxAnimLayer* animLayer = FbxAnimLayer::Create(Scene, string(animName + "_Layer").c_str());
		animStack->AddMember(animLayer);

		// for every bone...
		for (size_t i = 0; i < animations.m_KeyFrames.m_BoneFrames.Size(); ++i)
		{
			BoneFrames& bf = animations.m_KeyFrames.m_BoneFrames[i];
			FbxNode* boneNode = FindNode(bf.m_CRCchecksum);

			if (boneNode == nullptr)
			{
				Log("Could not find a Bone for CRC: " + std::to_string(bf.m_CRCchecksum), ELogType::Warning);
				continue;
			}

			// Translation
			FbxAnimCurve* tranCurveX = boneNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			FbxAnimCurve* tranCurveY = boneNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			FbxAnimCurve* tranCurveZ = boneNode->LclTranslation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

			tranCurveX->KeyModifyBegin();
			tranCurveY->KeyModifyBegin();
			tranCurveZ->KeyModifyBegin();
			for (size_t j = 0; j < bf.m_TranslationFrames.Size(); ++j)
			{
				TranslationFrame& tranFrame = bf.m_TranslationFrames[j];

				FbxTime time;
				time.SetSecondDouble(tranFrame.m_FrameIndex / anim.m_FrameRate);
				tranCurveX->KeySet(tranCurveX->KeyAdd(time), time, tranFrame.m_Translation.m_X, FbxAnimCurveDef::eInterpolationLinear);
				tranCurveY->KeySet(tranCurveY->KeyAdd(time), time, tranFrame.m_Translation.m_Y, FbxAnimCurveDef::eInterpolationLinear);
				tranCurveZ->KeySet(tranCurveZ->KeyAdd(time), time, tranFrame.m_Translation.m_Z, FbxAnimCurveDef::eInterpolationLinear);
			}
			tranCurveX->KeyModifyEnd();
			tranCurveY->KeyModifyEnd();
			tranCurveZ->KeyModifyEnd();

			// Rotation
			FbxAnimCurve* rotCurveX = boneNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_X, true);
			FbxAnimCurve* rotCurveY = boneNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Y, true);
			FbxAnimCurve* rotCurveZ = boneNode->LclRotation.GetCurve(animLayer, FBXSDK_CURVENODE_COMPONENT_Z, true);

			rotCurveX->KeyModifyBegin();
			rotCurveY->KeyModifyBegin();
			rotCurveZ->KeyModifyBegin();
			for (size_t j = 0; j < bf.m_RotationFrames.Size(); ++j)
			{
				RotationFrame& rotFrame = bf.m_RotationFrames[j];
				FbxVector4 rot = QuaternionToEuler(rotFrame.m_Rotation);

				FbxTime time;
				time.SetSecondDouble(rotFrame.m_FrameIndex / anim.m_FrameRate);
				rotCurveX->KeySet(rotCurveX->KeyAdd(time), time, (float)rot[0], FbxAnimCurveDef::eInterpolationLinear);
				rotCurveY->KeySet(rotCurveY->KeyAdd(time), time, (float)rot[1], FbxAnimCurveDef::eInterpolationLinear);
				rotCurveZ->KeySet(rotCurveZ->KeyAdd(time), time, (float)rot[2], FbxAnimCurveDef::eInterpolationLinear);
			}
			rotCurveX->KeyModifyEnd();
			rotCurveY->KeyModifyEnd();
			rotCurveZ->KeyModifyEnd();
		}
	}

	bool Converter::MATDToFBXMaterial(const MATD& material, FbxNode* meshNode, FbxLayer* layer, int& matIndex)
	{
		if (Manager == nullptr)
		{
			Log("FbxManager is NULL!", ELogType::Error);
			return false;
		}

		if (meshNode == nullptr)
		{
			Log("Given FbxNode is NULL!", ELogType::Error);
			return false;
		}

		// TODO:
		FbxLayerElementMaterial* materials = layer->GetMaterials();

		matIndex = meshNode->GetMaterialIndex(material.m_Name.m_Text.Buffer());

		// Create Material if non existent
		if (matIndex < 0)
		{
			FbxSurfacePhong* fbxMaterial = FbxSurfacePhong::Create(Scene, material.m_Name.m_Text.Buffer());
			fbxMaterial->Diffuse.Set(ColorToFBXColor(material.m_Data.m_Diffuse));
			fbxMaterial->Ambient.Set(ColorToFBXColor(material.m_Data.m_Ambient));
			fbxMaterial->Specular.Set(ColorToFBXColor(material.m_Data.m_Specular));

			FbxFileTexture* fbxTexture = FbxFileTexture::Create(Scene, material.m_Texture0.m_Text.Buffer());
			fbxTexture->SetFileName(material.m_Texture0.m_Text.Buffer()); // Resource file is in current directory.
			fbxTexture->SetTextureUse(FbxTexture::eStandard);
			fbxTexture->SetMappingType(FbxTexture::eUV);
			fbxTexture->SetMaterialUse(FbxFileTexture::eModelMaterial);

			fbxMaterial->Diffuse.ConnectSrcObject(fbxTexture);

			matIndex = meshNode->AddMaterial(fbxMaterial);
		}

		return true;
	}

	bool Converter::MODLToFBXMesh(MODL& model, MATL& materials, FbxNode* meshNode)
	{
		if (Manager == nullptr)
		{
			Log("FbxManager is NULL!", ELogType::Error);
			return false;
		}

		if (meshNode == nullptr)
		{
			Log("Given FbxNode is NULL!", ELogType::Error);
			return false;
		}

		FbxMesh* mesh = FbxMesh::Create(Manager, model.m_Name.m_Text.Buffer());
		FbxLayer* layer = mesh->GetLayer(0);
		if (layer == nullptr)
		{
			layer = mesh->GetLayer(mesh->CreateLayer());
		}

		vector<FbxVector4> vertices;
		vector<FbxVector4> normals;
		vector<FbxVector2> uvs;
		size_t vertexOffset = 0;

		vector<FbxCluster*> boneClusters;

		// crawl all Segments
		for (size_t i = 0; i < model.m_Geometry.m_Segments.Size(); ++i)
		{
			SEGM& segment = model.m_Geometry.m_Segments[i];

			if (segment.m_VertexList.m_Vertices.Size() == segment.m_NormalList.m_Normals.Size())
			{
				// grab the segments vertices, normals and UVs
				for (size_t j = 0; j < segment.m_VertexList.m_Vertices.Size(); ++j)
				{
					Vector3& v = segment.m_VertexList.m_Vertices[j];
					vertices.emplace_back(v.m_X, v.m_Y, v.m_Z);

					Vector3& n = segment.m_NormalList.m_Normals[j];
					normals.emplace_back(n.m_X, n.m_Y, n.m_Z);

					// UVs are optional
					if (j < segment.m_UVList.m_UVs.Size())
					{
						Vector2& uv = segment.m_UVList.m_UVs[j];
						uvs.emplace_back(uv.m_X, uv.m_Y);
					}
				}

				// convert MSH triangle strips to polygons
				segment.m_TriangleList.CalcPolygons();
				for (size_t j = 0; j < segment.m_TriangleList.m_Polygons.Size(); ++j)
				{
					auto& poly = segment.m_TriangleList.m_Polygons[j];

					uint32_t& mshMatIndex = segment.m_MaterialIndex.m_MaterialIndex;
					int fbxMatIndex = -1;

					if (mshMatIndex >= 0 && mshMatIndex < materials.m_Materials.Size())
					{
						MATD& mshMat = materials.m_Materials[segment.m_MaterialIndex.m_MaterialIndex];

						if ((ChunkFilter & EChunkFilter::Materials) == 0 && !MATDToFBXMaterial(mshMat, meshNode, layer, fbxMatIndex))
						{
							Log("Could not convert MSH Material '" + string(mshMat.m_Name.m_Text.Buffer()) + "' to FbxMaterial!", ELogType::Warning);
						}
					}
					else
					{
						Log("Material Index '" + std::to_string(mshMatIndex) + "' out of bounds " + std::to_string(materials.m_Materials.Size()), ELogType::Warning);
					}

					mesh->BeginPolygon(fbxMatIndex);
					for (size_t k = 0; k < poly.m_VertexIndices.Size(); ++k)
					{
						mesh->AddPolygon((int)(poly.m_VertexIndices[k] + vertexOffset));
					}
					mesh->EndPolygon();
				}

				// since in MSH vertices are local in their respective segments,
				// we have to store an offset because in FBX vertices are global
				vertexOffset += segment.m_VertexList.m_Vertices.Size();
			}
			else
			{
				Log("Inconsistent lengths of vertices and normals in Segment No: " + std::to_string(i), ELogType::Warning);
				mesh->Destroy(true);
				mesh = nullptr;
				return false;
			}
		}

		mesh->SetControlPointCount((int)vertices.size());
		for (size_t i = 0; i < vertices.size(); ++i)
		{
			mesh->SetControlPointAt(vertices[i], normals[i], i);
		}

		// Set UVs (if any)
		if (uvs.size() > 0)
		{
			FbxLayerElementUV* uvElement = FbxLayerElementUV::Create(mesh, "DiffuseUVs");
			uvElement->SetMappingMode(FbxGeometryElement::eByControlPoint);
			uvElement->SetReferenceMode(FbxGeometryElement::eDirect);
			uvElement->GetDirectArray().SetCount((int)uvs.size());

			for (size_t i = 0; i < uvs.size(); ++i)
			{
				uvElement->GetDirectArray().Add(uvs[i]);
			}
			layer->SetUVs(uvElement);
		}

		meshNode->SetNodeAttribute(mesh);
		meshNode->SetShadingMode(FbxNode::eTextureShading);
		return true;
	}

	bool Converter::MODLToFBXSkeleton(MODL& model, FbxNode* boneNode)
	{
		if (Manager == nullptr)
		{
			Log("FbxManager is NULL!", ELogType::Error);
			return false;
		}

		if (boneNode == nullptr)
		{
			Log("Given FbxNode is NULL!", ELogType::Error);
			return false;
		}

		EModelPurpose purpose = model.GetPurpose();
		FbxSkeleton* bone = FbxSkeleton::Create(Manager, model.m_Name.m_Text.Buffer());
		bone->Size.Set(1.0f);

		switch (purpose)
		{
			case EModelPurpose::Skeleton_Root:
			case EModelPurpose::Skeleton_BoneRoot:
				bone->SetSkeletonType(FbxSkeleton::eRoot);
				break;
			case EModelPurpose::Skeleton_BoneLimb:
			case EModelPurpose::Skeleton_BoneEnd:
				bone->SetSkeletonType(FbxSkeleton::eLimbNode);
				break;
			default:
				Log("No suitable Bone Type found for '" + string(model.m_Name.m_Text.Buffer()) + "' ! Model Type is: " + std::to_string((int)model.m_ModelType.m_ModelType) + "  Estimated Model Purpose is: " + ModelPurposeToString(purpose).Buffer(), ELogType::Warning);
				bone->Destroy();
				return false;
		}

		boneNode->SetNodeAttribute(bone);
		return true;
	}
}