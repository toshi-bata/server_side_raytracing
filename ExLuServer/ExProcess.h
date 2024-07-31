#pragma once
#include <A3DSDKIncludes.h>

#include "libconverter.h"
#include<string>
#include <vector>
#include <map>

using namespace Communicator;
using string_t = std::basic_string<A3DUniChar>;

class ExProcess
{
public:
	ExProcess();
	~ExProcess();

private:
	std::map<std::string, A3DAsmModelFile*> m_mModelFile;
	std::map<std::string, A3DEntity*> m_mPrcIdMap;
    A3DRWParamsLoadData m_sLoadData;
	Converter m_libConverter;
	Importer m_libImporter; // Import Initialization

public:
	bool Init();
	void Terminate();

	void SetOptions();
	void DeleteModelFile(const char* session_id);
	bool LoadFile(const char* session_id, const char* file_name, const char* sc_name);
	A3DAsmModelFile* GetModelFile(const char* session_id, A3DEntity*& pPrcIdMap);
};

