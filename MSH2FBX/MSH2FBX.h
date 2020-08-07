#pragma once
#include "pch.h"

namespace MSH2FBX
{
	using ConverterLib::Converter;
	using ConverterLib::EChunkFilter;
	using ConverterLib::LogCallback;
	using LibSWBF2::EModelPurpose;
	namespace fs = std::filesystem;

	static bool IsInProgress = false;

	void Log(const char* msg);
	void Log(const string& msg);
	void ReceiveLogFromConverter(const char* msg, const uint8_t type);
	void ShowProgress(const string& text, const float progress);
	void FinishProgress(string FinMsg);

	bool IsDirectory(const fs::path Path);
	vector<fs::path> GetFiles(const fs::path& Directory, const string& Extension, const bool recursive);
	vector<fs::path> GetFiles(const vector<fs::path>& Paths, const string& Extension, const bool recursive);

	bool ProcessMSH(fs::path filename, const bool overrideAnimName, Converter& converter, const bool createFBXFile);
}