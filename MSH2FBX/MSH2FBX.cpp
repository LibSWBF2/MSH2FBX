#include "pch.h"
#include "MSH2FBX.h"
#include "CLI11.hpp"
#include "Converter.h"
#include <math.h>

namespace MSH2FBX
{
	void Log(const char* msg)
	{
		std::cout << msg << std::endl;
	}

	void Log(const string& msg)
	{
		std::cout << msg << std::endl;
	}

	void LogEntry(LoggerEntry entry)
	{
		if (entry.m_Level >= ELogType::Warning)
		{
			Log(entry.m_Message.c_str());
		}
	}

	function<void(LoggerEntry)> LogCallback = LogEntry;

	string GetFileName(const string& path)
	{
		char sep = '\\';
		size_t i = path.rfind(sep, path.length());
		if (i != string::npos) {
			return(path.substr(i + 1, path.length() - i));
		}
		return("");
	}

	string RemoveFileExtension(const string& fileName)
	{
		size_t lastindex = fileName.find_last_of(".");

		if (lastindex == string::npos)
		{
			return fileName;
		}

		return fileName.substr(0, lastindex);
	}
}

// the main function must not lie inside a namespace
int main(int argc, char *argv[])
{
	using std::string;
	using std::vector;
	using std::map;
	using LibSWBF2::Logging::Logger;
	using LibSWBF2::Chunks::Mesh::MSH;
	using MSH2FBX::Converter;
	using MSH2FBX::EChunkFilter;
	using MSH2FBX::EModelPurpose;

	Logger::SetLogCallback(MSH2FBX::LogCallback);

	vector<string> files;
	vector<string> animations;
	vector<string> models;
	map<string, EModelPurpose> filterMap
	{
		// Meshes
		{"Mesh", EModelPurpose::Mesh},
		{"Mesh_Regular", EModelPurpose::Mesh_Regular},
		{"Mesh_Lowrez", EModelPurpose::Mesh_Lowrez},
		{"Mesh_Collision", EModelPurpose::Mesh_Collision},
		{"Mesh_VehicleCollision", EModelPurpose::Mesh_VehicleCollision},
		{"Mesh_ShadowVolume", EModelPurpose::Mesh_ShadowVolume},
		{"Mesh_TerrainCut", EModelPurpose::Mesh_TerrainCut},

		// Just Points
		{"Point", EModelPurpose::Point},
		{"Point_EmptyTransform", EModelPurpose::Point_EmptyTransform},
		{"Point_DummyRoot", EModelPurpose::Point_DummyRoot},
		{"Point_HardPoint", EModelPurpose::Point_HardPoint},

		// Skeleton
		{"Skeleton", EModelPurpose::Skeleton},
		{"Skeleton_Root", EModelPurpose::Skeleton_Root},
		{"Skeleton_BoneRoot", EModelPurpose::Skeleton_BoneRoot},
		{"Skeleton_BoneLimb", EModelPurpose::Skeleton_BoneLimb},
		{"Skeleton_BoneEnd", EModelPurpose::Skeleton_BoneEnd},
	};
	vector<string> filter;
	string fbxFileName = "";

	// Build up Command Line Parser
	CLI::App app
	{
		"--------------------------------------------------------------\n"
		"-------------------- MSH to FBX Converter --------------------\n"
		"--------------------------------------------------------------\n"
		"Web: https://github.com/Ben1138/MSH2FBX \n"
	};
	app.add_option("-f,--files", files, "MSH File Names importing all")->check(CLI::ExistingFile);
	app.add_option("-a,--animation", animations, "MSH File Names importing Animation Data only")->check(CLI::ExistingFile);
	app.add_option("-m,--model", models, "MSH File Names importing Model Data only")->check(CLI::ExistingFile);
	app.add_option("-n,--name", fbxFileName, "Name of the resulting FBX File (optional)")->check(CLI::ExistingFile);

	string filterOptionInfo = "What to ignore. Options are:\n";
	for (auto it = filterMap.begin(); it != filterMap.end(); ++it)
	{
		filterOptionInfo += "\t\t\t\t" + it->first + "\n";
	}
	app.add_option("-i,--ignore", filter, filterOptionInfo);

	CLI11_PARSE(app, argc, argv);

	// Do nothing if no msh files are given
	if (files.size() == 0 && animations.size() == 0 && models.size() == 0)
	{
		MSH2FBX::Log(app.help());
		return 0;
	}

	// allow everything by default
	Converter::ModelIgnoreFilter = (EModelPurpose)0;
	for (auto it = filter.begin(); it != filter.end(); ++it)
	{
		auto filterIT = filterMap.find(*it);
		if (filterIT != filterMap.end())
		{
			// ugly... |= operator does not work here
			Converter::ModelIgnoreFilter = (EModelPurpose)(Converter::ModelIgnoreFilter | filterIT->second);
		}
		else
		{
			MSH2FBX::Log("'"+*it+"' is not a valid filter option!");
		}
	}

	// if no FBX file name is specified, just take the first msh file name
	if (fbxFileName == "")
	{
		if (files.size() > 0)
		{
			fbxFileName = MSH2FBX::RemoveFileExtension(files[0]) + ".fbx";
		}
		else if (models.size() > 0)
		{
			fbxFileName = MSH2FBX::RemoveFileExtension(models[0]) + ".fbx";
		}
		else if (animations.size() > 0)
		{
			fbxFileName = MSH2FBX::RemoveFileExtension(animations[0]) + ".fbx";
		}
	}

	Converter::Start(fbxFileName);

	// Import Models first, ignoring Animations
	Converter::ChunkFilter = EChunkFilter::Animations;
	for (auto it = models.begin(); it != models.end(); ++it)
	{
		MSH* msh = MSH::Create();
		msh->ReadFromFile(*it);
		Converter::AddMSH(msh);
		MSH::Destroy(msh);
	}

	// Import complete Files second. These can include both, Models and Animations
	Converter::ChunkFilter = EChunkFilter::None;
	for (auto it = files.begin(); it != files.end(); ++it)
	{
		MSH* msh = MSH::Create();
		msh->ReadFromFile(*it);
		Converter::AddMSH(msh);
		MSH::Destroy(msh);
	}

	// Import Animations at last, so all Bones will be there
	Converter::ChunkFilter = EChunkFilter::Models;
	for (auto it = animations.begin(); it != animations.end(); ++it)
	{
		MSH* msh = MSH::Create();
		msh->ReadFromFile(*it);
		Converter::OverrideAnimName = MSH2FBX::GetFileName(MSH2FBX::RemoveFileExtension(*it));
		Converter::AddMSH(msh);
		MSH::Destroy(msh);
	}
	Converter::OverrideAnimName = "";

	Converter::Save();
	return 0;
}