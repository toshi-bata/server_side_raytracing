#define HOOPS_PRODUCT_PUBLISH_ADVANCED
#include <hoops_luminate_bridge/HoopsExLuminateBridge.h>
#include <REDFactory.h>
#include <REDITransformShape.h>
#include <REDIShape.h>
#include <REDImageTools.h>
#include <REDIMaterialController.h>
#include <REDIMaterialControllerProperty.h>

#include <hoops_luminate_bridge/LuminateRCTest.h>

namespace hoops_luminate_bridge {
    static double s_dUnit;

	HoopsLuminateBridgeEx::HoopsLuminateBridgeEx()
	{

	}

	HoopsLuminateBridgeEx::~HoopsLuminateBridgeEx()
	{

	}

    void HoopsLuminateBridgeEx::saveCameraState() {  }

    LuminateSceneInfoPtr HoopsLuminateBridgeEx::convertScene() { return convertExSceneToLuminate(m_pModelFile, m_pPrcIdMap); }

    bool HoopsLuminateBridgeEx::checkCameraChange()
    {
        //HPS::CameraKit camera;
        //m_viewSK.ShowCamera(camera);

        //bool hasChanged = m_hpsCamera != camera;
        bool hasChanged = false;

        //if (hasChanged)
        //    m_hpsCamera = camera;

        return hasChanged;
    }

    CameraInfo HoopsLuminateBridgeEx::getCameraInfo() { CameraInfo cameraInfo; return cameraInfo;/*return getHPSCameraInfo(m_viewSK);*/ }

    void HoopsLuminateBridgeEx::updateSelectedSegmentTransform()
    {
        //ConversionContextHPS* conversionDataHPS = (ConversionContextHPS*)m_conversionDataPtr.get();
        //SelectedHPSSegmentInfoPtr selectedHPSSegment = std::dynamic_pointer_cast<SelectedHPSSegmentInfo>(m_selectedSegment);

        //if (selectedHPSSegment == nullptr)
        //    return;

        //hoops_luminate_bridge::SegmentTransformShapeMap::iterator it =
        //    conversionDataHPS->segmentTransformShapeMap.find(selectedHPSSegment->m_serializedKeyPath);

        //if (it != conversionDataHPS->segmentTransformShapeMap.end()) {
        //    HPS::MatrixKit matrixKit;
        //    selectedHPSSegment->m_segmentKey.ShowModellingMatrix(matrixKit);

        //    float vizMatrix[16];
        //    matrixKit.ShowElements(vizMatrix);

        //    RED::Matrix redMatrix = RED::Matrix::IDENTITY;
        //    redMatrix.SetColumnMajorMatrix(vizMatrix);

        //    RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        //    RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();
        //    RED::ITransformShape* itransform = it->second->As<RED::ITransformShape>();
        //    itransform->SetMatrix(&redMatrix, iresourceManager->GetState());

        //    resetFrame();
        //}

        m_selectedSegmentTransformIsDirty = false;
    }

    std::vector<RED::Object*> HoopsLuminateBridgeEx::getSelectedLuminateMeshShapes()
    {
        //if (m_selectedSegment != nullptr) {
        //    ConversionContextHPS* conversionDataHPS = (ConversionContextHPS*)m_conversionDataPtr.get();
        //    SelectedHPSSegmentInfoPtr selectedHPSSegment = std::dynamic_pointer_cast<SelectedHPSSegmentInfo>(m_selectedSegment);

        //    hoops_luminate_bridge::SegmentMeshShapesMap::iterator it =
        //        conversionDataHPS->segmentMeshShapesMap.find(selectedHPSSegment->m_instanceID);

        //    if (it != conversionDataHPS->segmentMeshShapesMap.end()) {
        //        return it->second;
        //    }
        //}
        return std::vector<RED::Object*>();
    }

    RED::Object* HoopsLuminateBridgeEx::getSelectedLuminateTransformNode(char* a_node_name)
    {
        if (a_node_name != nullptr) {
            ConversionContextNode* conversionDataNode = (ConversionContextNode*)m_conversionDataPtr.get();

            if (0 == strcmp(a_node_name, "HL_floorPlane"))
            {
                if (nullptr != conversionDataNode->floorMesh)
                    return conversionDataNode->floorMesh;
            }
            else
            {
                if (0 < conversionDataNode->segmentTransformShapeMap.count(a_node_name))
                    return conversionDataNode->segmentTransformShapeMap[a_node_name];
            }
        }
        return nullptr;
    }

    RED::Color HoopsLuminateBridgeEx::getSelectedLuminateDeffuseColor(char* a_node_name)
    {
        if (a_node_name != nullptr) {
            ConversionContextNode* conversionDataNode = (ConversionContextNode*)m_conversionDataPtr.get();

            if (0 < conversionDataNode->segmentTransformShapeMap.count(a_node_name))
                return conversionDataNode->nodeDiffuseColorMap[a_node_name];
        }
        return RED::Color::GREY;
    }

    RED_RC HoopsLuminateBridgeEx::syncRootTransform()
    {
        //HPS::SegmentKey modelSegmentKey = m_modelSK;
        //HPS::MatrixKit modellingMatrix;
        //if (modelSegmentKey.ShowModellingMatrix(modellingMatrix)) {
        //    // get current matrix
        //    float vizMatrix[16];
        //    modellingMatrix.ShowElements(vizMatrix);

        //    RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        //    RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();
        //    LuminateSceneInfoPtr sceneInfo = m_conversionDataPtr;
        //    RED::ITransformShape* itransform = sceneInfo->rootTransformShape->As<RED::ITransformShape>();

        //    // Apply transform matrix.
        //    RED::Matrix redMatrix;
        //    redMatrix.SetColumnMajorMatrix(vizMatrix);

        //    // Restore left handedness transform.
        //    if (m_conversionDataPtr->viewHandedness == Handedness::LeftHanded) {
        //        redMatrix = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * redMatrix;
        //    }

        //    RC_CHECK(itransform->SetMatrix(&redMatrix, iresourceManager->GetState()));
        //    resetFrame();
        //}

        //m_rootTransformIsDirty = false;

        return RED_RC();
    }

    bool HoopsLuminateBridgeEx::addFloorMesh(const int pointCnt, const double* points, const int faceCnt, const int* faceList, const double* uvs)
    {
        ConversionContextNode* conversionDataNode = (ConversionContextNode*)m_conversionDataPtr.get();
        if (nullptr == conversionDataNode->rootTransformShape)
            return false;

        RED::ITransformShape* itransform = conversionDataNode->rootTransformShape->As<RED::ITransformShape>();

        RED::Object* resmgr = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresmgr = resmgr->As<RED::IResourceManager>();

        RED::Object* mesh = RED::Factory::CreateInstance(CID_REDMeshShape);
        RED::IMeshShape* imesh = mesh->As<RED::IMeshShape>();

        std::vector<float> fPoints(points, points + pointCnt);
        RC_TEST(imesh->SetArray(RED::MCL_VERTEX, fPoints.data(), pointCnt / 3, 3, RED::MFT_FLOAT, iresmgr->GetState()));
        std::vector<float>().swap(fPoints);

        RC_TEST(imesh->AddTriangles(faceList, faceCnt, iresmgr->GetState()));

        std::vector<double> normals;
        for (int i = 0; i < pointCnt / 3; i++)
        {
            normals.push_back(0.0);
            normals.push_back(0.0);
            normals.push_back(1.0);
        }
        RC_TEST(imesh->SetArray(RED::MCL_NORMAL, normals.data(), pointCnt / 3, 3, RED::MFT_FLOAT, iresmgr->GetState()));
        std::vector<double>().swap(normals);

        if (nullptr != uvs)
        {
            std::vector<float>().swap(m_floorUVArr);
            for (int i = 0; i < pointCnt / 3 * 2; i++)
                m_floorUVArr.push_back((float)uvs[i]);
            RC_TEST(imesh->SetArray(RED::MESH_CHANNEL::MCL_TEX0, m_floorUVArr.data(), pointCnt / 3, 2, RED::MFT_FLOAT, iresmgr->GetState()));
        }
        else
        {
            RC_TEST(imesh->BuildTextureCoordinates(RED::MESH_CHANNEL::MCL_TEX0, RED::MTCM_BOX, RED::Matrix::IDENTITY, iresmgr->GetState()));
        }

        RC_TEST(itransform->AddChild(mesh, RED_SHP_DAG_NO_UPDATE, iresmgr->GetState()));

        conversionDataNode->floorMesh = mesh;

        resetFrame();

        return true;
    }

    bool HoopsLuminateBridgeEx::deleteFloorMesh()
    {
        ConversionContextNode* conversionDataNode = (ConversionContextNode*)m_conversionDataPtr.get();
        if (nullptr == conversionDataNode->floorMesh)
            return false;

        RED::ITransformShape* itransform = conversionDataNode->rootTransformShape->As<RED::ITransformShape>();

        RED::Object* resmgr = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresmgr = resmgr->As<RED::IResourceManager>();

        RC_CHECK(itransform->RemoveChild(conversionDataNode->floorMesh, RED_SHP_DAG_NO_UPDATE, iresmgr->GetState()));

        conversionDataNode->floorMesh = nullptr;

        std::vector<float>().swap(m_floorUVArr);

        resetFrame();

        return true;
    }

    bool HoopsLuminateBridgeEx::updateFloorMaterial(const double* color, const char* texturePath, const double uvScale)
    {
        ConversionContextNode* conversionDataNode = (ConversionContextNode*)m_conversionDataPtr.get();
        if (nullptr == conversionDataNode->floorMesh)
            return false;

        RED::Object* floorMesh = conversionDataNode->floorMesh;
        RED::IMeshShape* imesh = floorMesh->As<RED::IMeshShape>();

        RED::Object* resmgr = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresmgr = resmgr->As<RED::IResourceManager>();

        // Meterial
        RED::Object* material = nullptr;
        RC_TEST(iresmgr->CreateMaterial(material, iresmgr->GetState()));
        RED::IMaterial* imaterial = material->As< RED::IMaterial >();

        float fR = color[0];
        float fG = color[1];
        float fB = color[2];
        float fA = color[3];

        RED::Color deffuseColor = RED::Color(fR, fG, fB, 1.f);
        RED::Color feflectionColor = RED::Color(0.25f, 0.25f, 0.25f, 1.f);
        RED::Color anisotropyColor = RED::Color(0.25f, 0.25f, 0.25f, 1.f);
        RED::Color transmissionColor = RED::Color(1.f - fA, 1.f - fA, 1.f - fA, 1.f - fA);

        resetFrame();

        // Set UV scale
        if (m_floorUVArr.size() && 0 < uvScale)
        {
            std::vector<float> uvArr;
            for (int i = 0; i < m_floorUVArr.size(); i++)
                uvArr.push_back(m_floorUVArr[i] * uvScale);
            RC_TEST(imesh->SetArray(RED::MESH_CHANNEL::MCL_TEX0, uvArr.data(), int(uvArr.size() / 2), 2, RED::MFT_FLOAT, iresmgr->GetState()));
        }

        // Load texture
        RED::Object* texture = NULL;
        if (strlen(texturePath)) {
            // Load a texture:
            RC_TEST(iresmgr->CreateImage2D(texture, iresmgr->GetState()));
            RC_TEST(RED::ImageTools::Load(texture, texturePath, RED::FMT_RGB, false, false, RED::TGT_TEX_2D, iresmgr->GetState()));

        }

        RC_TEST(imaterial->SetupRealisticMaterial(

            false,                                                         // Double sided
            true,                                                          // Fresnel
            deffuseColor, texture, RED::Matrix::IDENTITY, RED::MCL_TEX0,   // Diffusion
            feflectionColor, NULL, RED::Matrix::IDENTITY, RED::MCL_TEX0, // Reflection
            RED::Color::BLACK, FLT_MAX,                                    // Reflection fog
            false, false, NULL, RED::Matrix::IDENTITY,                     // Environment
            transmissionColor, NULL, RED::Matrix::IDENTITY, RED::MCL_TEX0, // Transmission
            0.0f, NULL, RED::Matrix::IDENTITY, RED::MCL_TEX0,              // Transmission glossiness
            2.3f, NULL, RED::Matrix::IDENTITY, RED::MCL_TEX0,              // IOR
            RED::Color::WHITE, 1.0f,                                       // Transmission scattering
            false, false,                                                  // Caustics
            anisotropyColor, NULL, RED::Matrix::IDENTITY, RED::MCL_TEX0, // Reflection anisotropy
            0.0f, NULL, RED::Matrix::IDENTITY, RED::MCL_TEX0,              // Reflection anisotropy orientation
            NULL, RED::Matrix::IDENTITY, RED::MCL_TEX0, RED::MCL_USER0,    // Bump
            0.0f, 0.0f, NULL, RED::Matrix::IDENTITY, RED::MCL_TEX0,        // Displacement
            NULL, &RED::LayerSet::ALL_LAYERS,                              // Layersets
            resmgr, iresmgr->GetState()));

        // Apply material if any.
        if (material != nullptr)
        {
            RC_TEST(floorMesh->As<RED::IShape>()->SetMaterial(material, iresmgr->GetState()));

            return true;
        }

        return false;
    }

    RED::Object* HoopsLuminateBridgeEx::getFloorMesh()
    {
        ConversionContextNode* conversionDataNode = (ConversionContextNode*)m_conversionDataPtr.get();

        return conversionDataNode->floorMesh;
    }

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
            matrix[12] = data.m_sOrigin.m_dX * s_dUnit;
            matrix[13] = data.m_sOrigin.m_dY * s_dUnit;
            matrix[14] = data.m_sOrigin.m_dZ * s_dUnit;
            matrix[15] = 1.;

            matrix[0] = data.m_sXVector.m_dX * data.m_sScale.m_dX * s_dUnit;
            matrix[1] = data.m_sXVector.m_dY * data.m_sScale.m_dX * s_dUnit;
            matrix[2] = data.m_sXVector.m_dZ * data.m_sScale.m_dX * s_dUnit;
            matrix[3] = 0.;

            matrix[4] = data.m_sYVector.m_dX * data.m_sScale.m_dY * s_dUnit;
            matrix[5] = data.m_sYVector.m_dY * data.m_sScale.m_dY * s_dUnit;
            matrix[6] = data.m_sYVector.m_dZ * data.m_sScale.m_dY * s_dUnit;
            matrix[7] = 0.;

            matrix[8] = dMirror * sZVector.m_dX * data.m_sScale.m_dZ * s_dUnit;
            matrix[9] = dMirror * sZVector.m_dY * data.m_sScale.m_dZ * s_dUnit;
            matrix[10] = dMirror * sZVector.m_dZ * data.m_sScale.m_dZ * s_dUnit;
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

    RealisticMaterialInfo getSegmentMaterialInfo(A3DGraphRgbColorData a_sColor,
        RED::Object* a_resourceManager,
        RealisticMaterialInfo const& a_baseMaterialInfo,
        ImageNameToLuminateMap& a_ioImageNameToLuminateMap,
        TextureNameImageNameMap& a_ioTextureNameImageNameMap,
        PBRToRealisticConversionMap& a_ioPBRToRealisticConversionMap)
    {
        RealisticMaterialInfo materialInfo = a_baseMaterialInfo;
        RED::Object* redImage;

        if (1)
        {
            std::string textureName;

            // Determine diffuse color.
            materialInfo.colorsByChannel[ColorChannels::DiffuseColor] = RED::Color(float(a_sColor.m_dRed), float(a_sColor.m_dGreen), float(a_sColor.m_dBlue), 1.f);
            textureName = "";

            // Determine transmission color.
            // Note that HPS transparency is determined by diffuse color alpha and the transmission channel
            // is for greyscale opacity texture. The transmission concept is not the same than in Luminate.

            RED::Color diffuseColor = materialInfo.colorsByChannel[ColorChannels::DiffuseColor];
            if (diffuseColor.A() != 1.0f) {
                float alpha = diffuseColor.A();
                materialInfo.colorsByChannel[ColorChannels::TransmissionColor] = RED::Color(1.0f - alpha);
            }

            // Retrieve the segment diffuse texture name if it exists
            //if (getFacesTexture(a_segmentKeyPath,
            //    HPS::Material::Channel::DiffuseTexture,
            //    materialInfo.textureNameByChannel[TextureChannels::DiffuseTexture])) {
            //    registerTexture(a_segmentKeyPath,
            //        materialInfo.textureNameByChannel[TextureChannels::DiffuseTexture].textureName,
            //        a_resourceManager,
            //        a_ioImageNameToLuminateMap,
            //        a_ioTextureNameImageNameMap,
            //        redImage);
            //}

            // Determine bump map
            //if (getFacesTexture(a_segmentKeyPath,
            //    HPS::Material::Channel::Bump,
            //    materialInfo.textureNameByChannel[TextureChannels::BumpTexture])) {
            //    registerTexture(a_segmentKeyPath,
            //        materialInfo.textureNameByChannel[TextureChannels::BumpTexture].textureName,
            //        a_resourceManager,
            //        a_ioImageNameToLuminateMap,
            //        a_ioTextureNameImageNameMap,
            //        redImage);
            //}
        }

        return materialInfo;
    }

    RED::Object* convertExMeshToREDMeshShape(const RED::State& a_state, A3DMeshData a_meshData)
    {
        RED_RC rc;

        RED::Object* result = RED::Factory::CreateInstance(CID_REDMeshShape);
        RED::IMeshShape* imesh = result->As<RED::IMeshShape>();

        std::vector<float> points(a_meshData.m_pdCoords, a_meshData.m_pdCoords + a_meshData.m_uiCoordSize);

        rc = imesh->SetArray(RED::MCL_VERTEX, points.data(), a_meshData.m_uiCoordSize / 3, 3, RED::MFT_FLOAT, a_state);

        std::vector<float>().swap(points);

        int triangleCount = 0;
        RED::Vector<int> indices;
        int cnt = 0;
        for (int i = 0; i < a_meshData.m_uiFaceSize; i++)
        {
            int faceCnt = a_meshData.m_puiTriangleCountPerFace[i];
            triangleCount += faceCnt;
            for (int j = 0; j < faceCnt * 3; j++)
            {
                indices.push_back(a_meshData.m_puiVertexIndicesPerFace[cnt++]);
            }
        }

        rc = imesh->AddTriangles(&indices[0], triangleCount, a_state);

        std::vector<float> normols(a_meshData.m_pdNormals, a_meshData.m_pdNormals + a_meshData.m_uiNormalSize);

        rc = imesh->SetArray(RED::MCL_NORMAL, normols.data(), a_meshData.m_uiNormalSize / 3, 3, RED::MFT_FLOAT, a_state);

        std::vector<float>().swap(normols);

        // Luminate need UV coordinates so we will build them
        rc = imesh->BuildTextureCoordinates(RED::MESH_CHANNEL::MCL_TEX0, RED::MTCM_BOX, RED::Matrix::IDENTITY, a_state);

        // Create the vertex information used for shading tangent-space generation.
        // The channels mentionned here must match the SetupRealisticMaterial bump channels.
        // We reserve MCL_TEX0 for 3DF texture coordinates (if any, so use another channel)         
        rc = imesh->BuildTextureCoordinates(RED::MCL_TEX7, RED::MTCM_BOX, RED::Matrix::IDENTITY, a_state);
        rc = imesh->BuildTangents(RED::MCL_USER0, RED::MCL_TEX7, a_state);

        return result;
    }

    void traverseTreeNode(RED::Object* a_resmgr, ConversionContextNode& a_ioConversionContext, 
        A3DTree* const hnd_tree, A3DTreeNode* const hnd_node, A3DMiscCascadedAttributes* pParentAttr, A3DPrcIdMap* a_pMap, RED::Object* modelTransformShape)
    {
        RED_RC rc;
        RED::IResourceManager* iresmgr = a_resmgr->As<RED::IResourceManager>();
        RED::ITransformShape* iModelTransform = modelTransformShape->As<RED::ITransformShape>();

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
            else if (0 == strcmp(pTypeMsg, "kA3DTypeRiBrepModel") || 0 == strcmp(pTypeMsg, "kA3DTypeRiPolyBrepModel"))
            {
                // Get node mesh
                A3DMeshData meshData;
                A3D_INITIALIZE_DATA(A3DMeshData, meshData);
                if (A3D_SUCCESS != A3DRiComputeMesh(pEntity, pParentAttr, &meshData, nullptr))
                    return;

                if (0 == meshData.m_uiCoordSize || 0 == meshData.m_uiFaceSize)
                    return;

                // Get PRC ID
                A3DTreeNode* parent_node;
                A3DTreeNodeGetParent(hnd_tree, hnd_node, &parent_node);

                A3DEntity* pParentEntity = nullptr;
                A3DTreeNodeGetEntity(parent_node, &pParentEntity);

                A3DPrcId prcId;
                iRet = A3DPrcIdMapFindId(a_pMap, pEntity, pParentEntity, &prcId);

                A3DGraphRgbColorData sColor;
                A3DUns32 uiColorIndex = styleData.m_uiRgbColorIndex;
                if (A3D_DEFAULT_STYLE_INDEX != uiColorIndex)
                {
                    A3D_INITIALIZE_DATA(A3DGraphRgbColorData, sColor);
                    A3DGlobalGetGraphRgbColorData(uiColorIndex, &sColor);
                }

                // Create Luminate matrix
                RED::Matrix redMatrix = RED::Matrix::IDENTITY;
                redMatrix.SetColumnMajorMatrix(matrix);

                // Create Luminate material
                RED::Object* material = nullptr;
                std::vector<RED::Object*> meshShapes;

                RealisticMaterialInfo materialInfo = getSegmentMaterialInfo(sColor,
                    a_resmgr,
                    a_ioConversionContext.defaultMaterialInfo,
                    a_ioConversionContext.imageNameToLuminateMap,
                    a_ioConversionContext.textureNameImageNameMap,
                    a_ioConversionContext.pbrToRealisticConversionMap);

                material = createREDMaterial(materialInfo,
                    a_resmgr,
                    a_ioConversionContext.imageNameToLuminateMap,
                    a_ioConversionContext.textureNameImageNameMap,
                    a_ioConversionContext.materials);

                RED::Object* shape = nullptr;
                shape = convertExMeshToREDMeshShape(iresmgr->GetState(), meshData);

                if (shape != nullptr)
                    meshShapes.push_back(shape);

                //////////////////////////////////////////
                // Create RED transform shape associated to segment
                //////////////////////////////////////////

                RED::Object* transform = RED::Factory::CreateInstance(CID_REDTransformShape);
                RED::ITransformShape* itransform = transform->As<RED::ITransformShape>();

                // Register transform shape associated to the segment.
                a_ioConversionContext.segmentTransformShapeMap[prcId] = transform;

                // DiffuseColor color.
                RED::Color deffuseColor = RED::Color(float(sColor.m_dRed), float(sColor.m_dGreen), float(sColor.m_dBlue), 1.f);
                a_ioConversionContext.nodeDiffuseColorMap[prcId] = deffuseColor;

                transform->SetID(prcId);

                // Apply transform matrix.
                RC_CHECK(itransform->SetMatrix(&redMatrix, iresmgr->GetState()));

                // Apply material if any.
                if (material != nullptr)
                    RC_CHECK(transform->As<RED::IShape>()->SetMaterial(material, iresmgr->GetState()));

                // Add geometry shapes if any.
                for (RED::Object* meshShape : meshShapes)
                    RC_CHECK(itransform->AddChild(meshShape, RED_SHP_DAG_NO_UPDATE, iresmgr->GetState()));

                iModelTransform->AddChild(transform, RED_SHP_DAG_NO_UPDATE, iresmgr->GetState());

                A3DRiComputeMesh(nullptr, nullptr, &meshData, nullptr);
                A3DTreeNodeGetEntity(nullptr, &pParentEntity);
                A3DTreeNodeGetParent(nullptr, nullptr, &parent_node);

            }

            // Get child nodes
            A3DUns32 n_child_nodes = 0;
            A3DTreeNode** child_nodes = nullptr;
            iRet = A3DTreeNodeGetChildren(hnd_tree, hnd_node, &n_child_nodes, &child_nodes);

            for (A3DUns32 n = 0; n < n_child_nodes; ++n)
            {
                traverseTreeNode(a_resmgr, a_ioConversionContext, hnd_tree, child_nodes[n], pAttr, a_pMap, modelTransformShape);
            }
            A3DTreeNodeGetChildren(0, 0, &n_child_nodes, &child_nodes);
        }
    }

    LuminateSceneInfoPtr convertExSceneToLuminate(A3DAsmModelFile* pModelFile, A3DEntity* pMap)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Create conversion root shape
        //////////////////////////////////////////

        RED::Object* rootTransformShape = RED::Factory::CreateInstance(CID_REDTransformShape);
        RED::ITransformShape* irootTtransform = rootTransformShape->As<RED::ITransformShape>();

        RED::Object* modelTransformShape = RED::Factory::CreateInstance(CID_REDTransformShape);
        RED::ITransformShape* iModelTransform = modelTransformShape->As<RED::ITransformShape>();

        irootTtransform->AddChild(modelTransformShape, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState());

        //////////////////////////////////////////
        // Manage scene handedness.
        // Luminate is working with a right-handed coordinate system,
        // thus if the input scene is left-handed we must apply
        // a transformation on the conversion root to go right-handed.
        //////////////////////////////////////////

        Handedness viewHandedness = Handedness::RightHanded;

        if (viewHandedness == Handedness::LeftHanded) {
            //RC_CHECK(itransform->SetMatrix(&HoopsLuminateBridge::s_leftHandedToRightHandedMatrix, iresourceManager->GetState()));
        }

        //////////////////////////////////////////
        // Create the default material definition.
        //////////////////////////////////////////

        RealisticMaterialInfo defaultMaterialInfo = createDefaultRealisticMaterialInfo();

        //////////////////////////////////////////
        // Initialize the conversion context which will hold
        // current conversion data state during conversion.
        //////////////////////////////////////////

        ConversionContextNodePtr sceneInfoPtr = std::make_shared<ConversionContextNode>();
        sceneInfoPtr->rootTransformShape = rootTransformShape;
        sceneInfoPtr->modelTransformShape = modelTransformShape;
        sceneInfoPtr->defaultMaterialInfo = defaultMaterialInfo;
        sceneInfoPtr->viewHandedness = viewHandedness;

        //////////////////////////////////////////
        // Proceed with node tree traversal to convert scene
        //////////////////////////////////////////

        A3DStatus iRet;
        // Get unit
        A3DAsmModelFileData sData;
        A3D_INITIALIZE_DATA(A3DAsmModelFileData, sData);
        A3DAsmModelFileGet(pModelFile, &sData);
        s_dUnit = sData.m_dUnit;

        // Traverse model
        A3DTree* tree = 0;
        iRet = A3DTreeCompute(pModelFile, &tree, 0);

        A3DTreeNode* root_node = 0;
        iRet = A3DTreeGetRootNode(tree, &root_node);

        A3DMiscCascadedAttributes* pAttr;
        A3DMiscCascadedAttributesCreate(&pAttr);
        
        traverseTreeNode(resourceManager, *sceneInfoPtr, tree, root_node, pAttr, (A3DPrcIdMap*)pMap, modelTransformShape);

        iRet = A3DTreeCompute(nullptr, &tree, nullptr);

        return sceneInfoPtr;
    }

}