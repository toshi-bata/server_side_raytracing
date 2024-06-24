#define INITIALIZE_A3D_API
#include "ExProcess.h"

#include <iterator>
#include <sstream>

A3DVector3dData cross_product(const A3DVector3dData& X, const A3DVector3dData& Y)
{
    A3DVector3dData Z;
    Z.m_dX = X.m_dY * Y.m_dZ - X.m_dZ * Y.m_dY;
    Z.m_dY = X.m_dZ * Y.m_dX - X.m_dX * Y.m_dZ;
    Z.m_dZ = X.m_dX * Y.m_dY - X.m_dY * Y.m_dX;
    return Z;
}

int get_matrix(const A3DMiscTransformation* transfo, double* matrix)
{
    A3DEEntityType type = kA3DTypeUnknown;
    A3DEntityGetType(transfo, &type);
    if (type == kA3DTypeMiscCartesianTransformation)
    {
        A3DMiscCartesianTransformationData data;
        A3D_INITIALIZE_DATA(A3DMiscCartesianTransformationData, data);

        if (A3DMiscCartesianTransformationGet(transfo, &data))
            return 1;
        const auto sZVector = cross_product(data.m_sXVector, data.m_sYVector);
        const double dMirror = (data.m_ucBehaviour & kA3DTransformationMirror) ? -1. : 1.;
        matrix[12] = data.m_sOrigin.m_dX;
        matrix[13] = data.m_sOrigin.m_dY;
        matrix[14] = data.m_sOrigin.m_dZ;
        matrix[15] = 1.;

        matrix[0] = data.m_sXVector.m_dX * data.m_sScale.m_dX;
        matrix[1] = data.m_sXVector.m_dY * data.m_sScale.m_dX;
        matrix[2] = data.m_sXVector.m_dZ * data.m_sScale.m_dX;
        matrix[3] = 0.;

        matrix[4] = data.m_sYVector.m_dX * data.m_sScale.m_dY;
        matrix[5] = data.m_sYVector.m_dY * data.m_sScale.m_dY;
        matrix[6] = data.m_sYVector.m_dZ * data.m_sScale.m_dY;
        matrix[7] = 0.;

        matrix[8] = dMirror * sZVector.m_dX * data.m_sScale.m_dZ;
        matrix[9] = dMirror * sZVector.m_dY * data.m_sScale.m_dZ;
        matrix[10] = dMirror * sZVector.m_dZ * data.m_sScale.m_dZ;
        matrix[11] = 0.;

        if (A3DMiscCartesianTransformationGet(nullptr, &data))
            return 1;
        return 0;
    }
    else
    {
        return 1;
    }
}

ExProcess::ExProcess()
{
}

ExProcess::~ExProcess()
{
    Terminate();
}

bool ExProcess::Init()
{
    A3DStatus iRet;
#ifndef _MSC_VER
    if (!A3DSDKLoadLibrary(""))
        return false;
#else

    wchar_t bin_dir[2048];
#ifdef _DEBUG
    std::wstring buffer;
    buffer.resize(2048);
    if (GetEnvironmentVariable(L"HEXCHANGE_INSTALL_DIR", &buffer[0], static_cast<DWORD>(buffer.size())))
    {
        swprintf(bin_dir, _T("%s/bin/win64_v142"), buffer.data());
    }
#else
    swprintf(bin_dir, _T(""));
#endif
    wprintf(_T("Exchange bin dir=\"%s\"\n"), bin_dir);
    if (!A3DSDKLoadLibrary(bin_dir))
        return false;

#endif
    iRet = A3DLicPutUnifiedLicense(HOOPS_LICENSE);
    if (iRet != A3D_SUCCESS)
        return false;

    A3DInt32 iMajorVersion = 0, iMinorVersion = 0;
    iRet = A3DDllGetVersion(&iMajorVersion, &iMinorVersion);
    if (iRet != A3D_SUCCESS)
        return false;

    iRet = A3DDllInitialize(A3D_DLL_MAJORVERSION, A3D_DLL_MINORVERSION);
    if (iRet != A3D_SUCCESS)
        return iRet;

    // Init libconverter
    m_libConverter.Init(HOOPS_LICENSE);

    if (!m_libImporter.Init(&m_libConverter))
        return false;

    return true;
}

void ExProcess::Terminate()
{
    if (A3D_SUCCESS == A3DDllTerminate())
    {
        A3DSDKUnloadLibrary();
        printf("HOOPS Exchanged Unloaded\n");
    }

}

void ExProcess::SetOptions(const bool entityIds, const bool sewModel, const double sewingTol)
{
    m_bAddEntityIds = entityIds;
    m_bSewModel = sewModel;
    m_dSewingTol = sewingTol;

    // Init import options
    A3D_INITIALIZE_DATA(A3DRWParamsLoadData, m_sLoadData);
    m_sLoadData.m_sGeneral.m_bReadSolids = true;
    m_sLoadData.m_sGeneral.m_bReadSurfaces = true;
    m_sLoadData.m_sGeneral.m_bReadWireframes = true;
    m_sLoadData.m_sGeneral.m_bReadPmis = true;
    m_sLoadData.m_sGeneral.m_bReadAttributes = true;
    m_sLoadData.m_sGeneral.m_bReadHiddenObjects = false;
    m_sLoadData.m_sGeneral.m_bReadConstructionAndReferences = false;
    m_sLoadData.m_sGeneral.m_bReadActiveFilter = false;
    m_sLoadData.m_sGeneral.m_eReadingMode2D3D = kA3DRead_3D;
    m_sLoadData.m_sGeneral.m_eReadGeomTessMode = kA3DReadGeomAndTess;
    m_sLoadData.m_sGeneral.m_eDefaultUnit = kA3DUnitUnknown;
    m_sLoadData.m_sTessellation.m_eTessellationLevelOfDetail = kA3DTessLODMedium;
    m_sLoadData.m_sAssembly.m_bUseRootDirectory = true;
    m_sLoadData.m_sMultiEntries.m_bLoadDefault = true;
    m_sLoadData.m_sPmi.m_bAlwaysSubstituteFont = false;
    m_sLoadData.m_sPmi.m_pcSubstitutionFont = (char*)"Myriad CAD";

    m_sLoadData.m_sSpecifics.m_sRevit.m_eMultiThreadedMode = kA3DRevitMultiThreadedMode_Disabled;
}

void ExProcess::DeleteModelFile(const char* session_id)
{
    // Delete loaded modelFile
    if (1 == m_mModelFile.count(session_id))
    {
        A3DAsmModelFile* pModelFile = m_mModelFile[session_id];
        m_mModelFile.erase(session_id);

        A3DStatus iRet = A3DAsmModelFileDelete(pModelFile);
        if (iRet == A3D_SUCCESS)
            printf("ModelFile was removed.\n");
    }
}

std::vector<float> ExProcess::LoadFile(const char* session_id, const char* file_name, const char* sc_name)
{
    std::vector<float> floatArray;
    
    A3DStatus iRet;

    A3DAsmModelFile* pModelFile;

    iRet = A3DAsmModelFileLoadFromFile(file_name, &m_sLoadData, &pModelFile);

    if (iRet != A3D_SUCCESS)
    {
        printf("File loading failed: %d\n", iRet);
        return floatArray;
    }
    printf("Model was loaded\n");
    m_mModelFile.insert(std::make_pair(session_id, pModelFile));

    // Sewing
    if (m_bSewModel)
    {
        A3DSewOptionsData sewOption;
        A3D_INITIALIZE_DATA(A3DSewOptionsData, sewOption);
        iRet = A3DAsmModelFileSew(&pModelFile, m_dSewingTol, &sewOption);
    }

    // HC libconverter
    if (!m_libImporter.Load(pModelFile))
        return floatArray;

    Exporter exporter; // Export Initialization
    if (!exporter.Init(&m_libImporter))
        return floatArray;

    SC_Export_Options exportOptions; // Export Stream Cache Model
    exportOptions.sc_create_scz = true;
    exportOptions.export_attributes = true;

    if (!exporter.WriteSC(sc_name, nullptr, exportOptions))
        return floatArray;
    printf("SC model was exported\n");
 
    // Get unit
    A3DAsmModelFileData sData;
    A3D_INITIALIZE_DATA(A3DAsmModelFileData, sData);
    iRet = A3DAsmModelFileGet(pModelFile, &sData);
    floatArray.push_back(sData.m_dUnit);

    // Retrieve physical properties
    A3DPhysicalPropertiesData sPhysPropsData;
    A3D_INITIALIZE_DATA(A3DPhysicalPropertiesData, sPhysPropsData);
    iRet = A3DComputeModelFilePhysicalProperties(pModelFile, &sPhysPropsData);

    floatArray.push_back(sPhysPropsData.m_dSurface);
    floatArray.push_back(sPhysPropsData.m_dVolume);
    floatArray.push_back(sPhysPropsData.m_sGravityCenter.m_dX);
    floatArray.push_back(sPhysPropsData.m_sGravityCenter.m_dY);
    floatArray.push_back(sPhysPropsData.m_sGravityCenter.m_dZ);

    // Compute bounding box
    A3DBoundingBoxData sBoundingBox;
    A3D_INITIALIZE_DATA(A3DBoundingBoxData, sBoundingBox);
    iRet = A3DMiscComputeBoundingBox(pModelFile, 0, &sBoundingBox);
    double dX = sBoundingBox.m_sMax.m_dX - sBoundingBox.m_sMin.m_dX;
    double dY = sBoundingBox.m_sMax.m_dY - sBoundingBox.m_sMin.m_dY;
    double dZ = sBoundingBox.m_sMax.m_dZ - sBoundingBox.m_sMin.m_dZ;

    floatArray.push_back(dX);
    floatArray.push_back(dY);
    floatArray.push_back(dZ);

    iRet = A3DAsmModelFileGet(NULL, &sData);

    return floatArray;
}

void ExProcess::traverseTree(A3DTree* const hnd_tree, A3DTreeNode* const hnd_node, A3DMiscCascadedAttributes* pParentAttr)
{
    A3DStatus iRet;

    // Get node net matrix
    A3DMiscTransformation* transf;
    iRet = A3DTreeNodeGetNetTransformation(hnd_node, &transf);
    
    double matrix[] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    get_matrix(transf, matrix);

    // Get node net attribute
    A3DGraphStyleData styleData;
    A3D_INITIALIZE_DATA(A3DGraphStyleData, styleData);
    iRet = A3DTreeNodeGetNetStyle(hnd_node, &styleData);

    A3DEntity* pEntity = nullptr;
    A3DTreeNodeGetEntity(hnd_node, &pEntity);

    A3DEEntityType eType = kA3DTypeUnknown;
    if (A3D_SUCCESS == A3DEntityGetType(pEntity, &eType))
    {
        A3DMiscCascadedAttributes* pAttr = nullptr;

        const A3DUTF8Char* pTypeMsg = A3DMiscGetEntityTypeMsg(eType);

        if (0 == strcmp(pTypeMsg, "kA3DTypeAsmProductOccurrence"))
        {
            iRet = A3DMiscCascadedAttributesCreate(&pAttr);
            iRet = A3DMiscCascadedAttributesPush(pAttr, pEntity, pParentAttr);
        }
        else if (0 == strcmp(pTypeMsg, "kA3DTypeRiBrepModel"))
        {
            // Get node mesh
            A3DMeshData mesh_data;
            A3D_INITIALIZE_DATA(A3DMeshData, mesh_data);
            if (A3D_SUCCESS != A3DRiComputeMesh(pEntity, pParentAttr, &mesh_data, nullptr))
                return;

            if (0 == mesh_data.m_uiCoordSize || 0 == mesh_data.m_uiFaceSize)
                return;

            MeshPropaties meshProps;
            meshProps.meshData = mesh_data;

            // Get parent node name
            A3DTreeNode* parent_node;
            A3DTreeNodeGetParent(hnd_tree, hnd_node, &parent_node);

            A3DUTF8Char* parent_name = nullptr;
            if (A3D_SUCCESS != A3DTreeNodeGetName(parent_node, &parent_name))
                parent_name = nullptr;

            meshProps.name = parent_name;

            std::copy(std::begin(matrix), std::end(matrix), std::begin(meshProps.matrix));

            A3DUns32 uiColorIndex = styleData.m_uiRgbColorIndex;
            if (A3D_DEFAULT_STYLE_INDEX != uiColorIndex)
            {
                A3D_INITIALIZE_DATA(A3DGraphRgbColorData, meshProps.color);
                A3DGlobalGetGraphRgbColorData(uiColorIndex, &meshProps.color);
            }

            m_aMeshProps.push_back(meshProps);
        }

        // Get child nodes
        A3DUns32 n_child_nodes = 0;
        A3DTreeNode** child_nodes = nullptr;
        iRet = A3DTreeNodeGetChildren(hnd_tree, hnd_node, &n_child_nodes, &child_nodes);

        for (A3DUns32 n = 0; n < n_child_nodes; ++n)
        {
            traverseTree(hnd_tree, child_nodes[n], pAttr);
        }
        A3DTreeNodeGetChildren(0, 0, &n_child_nodes, &child_nodes);
    }
}

std::vector<MeshPropaties> ExProcess::GetModelMesh(const char* session_id)
{
    A3DStatus iRet;

    m_aMeshProps.clear();

    if (0 == m_mModelFile.count(session_id))
        return m_aMeshProps;

    A3DAsmModelFile* pModelFile = m_mModelFile[session_id];

    // Traverse model
    A3DTree* tree = 0;
    iRet = A3DTreeCompute(pModelFile, &tree, 0);

    A3DTreeNode* root_node = 0;
    iRet = A3DTreeGetRootNode(tree, &root_node);

    A3DMiscCascadedAttributes* pAttr;
    A3DMiscCascadedAttributesCreate(&pAttr);

    traverseTree(tree, root_node, pAttr);

    return m_aMeshProps;
}
