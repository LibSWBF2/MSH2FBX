#pragma once

namespace ConverterLib
{
	using namespace LibSWBF2::Chunks::MSH;
	using namespace LibSWBF2::Types;
	using LibSWBF2::CRC;
	using LibSWBF2::CRCChecksum;
	using LibSWBF2::ELogType;
	using LibSWBF2::EModelPurpose;
	using LibSWBF2::Logging::Logger;
	using LibSWBF2::Logging::LoggerEntry;
	namespace fs = std::filesystem;

	// Bit Flags
	// Set a flag for all types you DON't want to be handled (filtered out)
	enum EChunkFilter : uint8_t
	{
		None = 0,
		Materials = 1,
		Models = 2,			// includes empty nodes, bones, etc.
		Animations = 4,
		Weights = 8
	};

	typedef void(*LogCallback)(const char* msg, const uint8_t type);

	class Converter
	{
	public:
		Converter();
		Converter(const fs::path& fbxFileName);
		Converter(const Converter& converter) = delete;
		Converter(const Converter&& converter) = delete;
		~Converter();

		// In filters, you specify what you DON'T want
		EModelPurpose ModelIgnoreFilter = EModelPurpose::Miscellaneous;
		EChunkFilter ChunkFilter = EChunkFilter::None;
		string OverrideAnimName = "";
		bool bEmptyMeshes = false;
		bool bPrintHierachy = false;
		fs::path BaseposeMSH = "";

		static void SetLogCallback(const LogCallback Callback);
		bool Start(const fs::path& fbxFileName);
		bool AddMSH(const fs::path& mshFileName);
		bool AddMSH(MSH* msh);
		bool SaveFBX();
		bool ClearFBXScene();
		void Close();

	private:
		map<MODL*, FbxNode*> MODLToFbxNode;
		map<CRCChecksum, FbxNode*> CRCToFbxNode;

		FbxNode* FindNode(MODL* model);
		FbxNode* FindNode(const CRCChecksum checksum);
		FbxDouble3 ColorToFBXColor(const Color& color);
		FbxDouble4 QuaternionToEuler(const Vector4& Quaternion);
		void ApplyTransform(FbxNode* modelNode, const Vector3& Translation, const Vector4& Rotation);
		void ApplyTransform(FbxNode* modelNode, const Vector3& Translation, const Vector4& Rotation, const Vector3& Scale);
		void MSHToFBXScene();
		void ANM2ToFBXAnimations(ANM2& animations);
		void WGHTToFBXSkin(WGHT& weights, const ENVL& envelope, const FbxAMatrix& matrixMeshNode, const size_t vertexOffset, map<MODL*, FbxCluster*>& BoneToCluster);
		bool MATDToFBXMaterial(const MATD& material, FbxNode* meshNode, FbxLayer* layer, int& matIndex);
		bool MODLToFBXMesh(MODL& model, MATL& materials, FbxNode* meshNode);
		bool MODLToFBXSkeleton(MODL& model, FbxNode* boneNode);
		void CheckHierarchy();

		// Current State
		bool bRunning = false;
		fs::path FbxFilePath;
		MSH* Mesh = nullptr;
		FbxScene* Scene = nullptr;
		FbxManager* Manager = nullptr;
		FbxPose* Bindpose = nullptr;
		MSH* Basepose = nullptr;

		// Logging
		static void ReceiveLogFromLib(const LoggerEntry* entry);
		static void Log(const string& msg, ELogType type);
		static LogCallback OnLogCallback;
	};
}