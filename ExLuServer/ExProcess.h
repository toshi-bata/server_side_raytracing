#pragma once
#include <A3DSDKIncludes.h>

#include "libconverter.h"
#include<string>
#include <vector>
#include <map>

using namespace Communicator;
using string_t = std::basic_string<A3DUniChar>;

struct MeshPropaties
{
	A3DUTF8Char* name;
	A3DMeshData meshData;
	double matrix[16];
	A3DGraphRgbColorData color;
};

class ExProcess
{
public:
	ExProcess();
	~ExProcess();

private:
	std::map<std::string, A3DAsmModelFile*> m_mModelFile;
    A3DRWParamsLoadData m_sLoadData;
	Converter m_libConverter;
	Importer m_libImporter; // Import Initialization
	bool m_bAddEntityIds;
	bool m_bSewModel;
	double m_dSewingTol;
	std::vector<MeshPropaties> m_aMeshProps;
	
	void traverseTree(A3DTree* const hnd_tree, A3DTreeNode* const hnd_node, A3DMiscCascadedAttributes* pParentAttr);

public:
	bool Init();
	void Terminate();

	void SetOptions();
	void DeleteModelFile(const char* session_id);
	std::vector<float> LoadFile(const char* session_id, const char* file_name, const char* sc_name);
	std::vector<MeshPropaties> GetModelMesh(const char* session_id);
};

