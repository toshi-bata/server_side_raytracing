#include <hoops_luminate_bridge/HoopsExLuminateBridge.h>
#include <REDFactory.h>
#include <REDITransformShape.h>
#include <REDIShape.h>
#include <REDImageTools.h>
#include <REDIMaterialController.h>
#include <REDIMaterialControllerProperty.h>

#include <hoops_luminate_bridge/LuminateRCTest.h>

namespace hoops_luminate_bridge {
	HoopsLuminateBridgeEx::HoopsLuminateBridgeEx()
	{

	}

	HoopsLuminateBridgeEx::~HoopsLuminateBridgeEx()
	{

	}

    void HoopsLuminateBridgeEx::saveCameraState() { /*m_viewSK.ShowCamera(m_hpsCamera);*/ }

    LuminateSceneInfoPtr HoopsLuminateBridgeEx::convertScene() { return convertExSceneToLuminate(m_aMeshProps); }

    bool HoopsLuminateBridgeEx::checkCameraChange()
    {
        //HPS::CameraKit camera;
        //m_viewSK.ShowCamera(camera);

        //bool hasChanged = m_hpsCamera != camera;

        //if (hasChanged)
        //    m_hpsCamera = camera;

        //return hasChanged;
        return false;
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

    LuminateSceneInfoPtr convertExSceneToLuminate(std::vector<MeshPropaties> aMeshProps)
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
        RED::ITransformShape* itransform = modelTransformShape->As<RED::ITransformShape>();

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

        for (MeshPropaties meshProps : aMeshProps)
        {
            RED::Object* result = convertNordTree(resourceManager, *sceneInfoPtr, meshProps);

            if (result)
                itransform->AddChild(result, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState());
        }

        return sceneInfoPtr;
    }

    RED::Object* convertNordTree(RED::Object* a_resourceManager, ConversionContextNode& a_ioConversionContext, MeshPropaties meshProps)
    {
        RED_RC rc;

        RED::IResourceManager* iresourceManager = a_resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Retrieve segment transform matrix it has one.
        // We want here the individual matrix, not the net one
        // because we benifit of Luminate transform stack as well.
        //////////////////////////////////////////

        RED::Matrix redMatrix = RED::Matrix::IDENTITY;
        redMatrix.SetColumnMajorMatrix(meshProps.matrix);

        RED::Object* material = nullptr;
        std::vector<RED::Object*> meshShapes;

        RealisticMaterialInfo materialInfo = getSegmentMaterialInfo(meshProps,
            a_resourceManager,
            a_ioConversionContext.defaultMaterialInfo,
            a_ioConversionContext.imageNameToLuminateMap,
            a_ioConversionContext.textureNameImageNameMap,
            a_ioConversionContext.pbrToRealisticConversionMap);

        material = createREDMaterial(materialInfo,
            a_resourceManager,
            a_ioConversionContext.imageNameToLuminateMap,
            a_ioConversionContext.textureNameImageNameMap,
            a_ioConversionContext.materials);

        RED::Object* shape = nullptr;
        shape = convertExMeshToREDMeshShape(iresourceManager->GetState(), meshProps.meshData);

        if (shape != nullptr)
            meshShapes.push_back(shape);

        //////////////////////////////////////////
        // Create RED transform shape associated to segment
        //////////////////////////////////////////

        RED::Object* transform = RED::Factory::CreateInstance(CID_REDTransformShape);
        RED::ITransformShape* itransform = transform->As<RED::ITransformShape>();

        // Register transform shape associated to the segment.
        a_ioConversionContext.segmentTransformShapeMap[meshProps.name] = transform;

        // DiffuseColor color.
        RED::Color deffuseColor = RED::Color(float(meshProps.color.m_dRed), float(meshProps.color.m_dGreen), float(meshProps.color.m_dBlue), 1.f);
        a_ioConversionContext.nodeDiffuseColorMap[meshProps.name] = deffuseColor;

        transform->SetID(meshProps.name);

        // Apply transform matrix.
        RC_CHECK(itransform->SetMatrix(&redMatrix, iresourceManager->GetState()));

        // Apply material if any.
        if (material != nullptr)
            RC_CHECK(transform->As<RED::IShape>()->SetMaterial(material, iresourceManager->GetState()));

        // Add geometry shapes if any.
        for (RED::Object* meshShape : meshShapes)
            RC_CHECK(itransform->AddChild(meshShape, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState()));

        return transform;
    }

    RealisticMaterialInfo getSegmentMaterialInfo(MeshPropaties meshProps,
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
            materialInfo.colorsByChannel[ColorChannels::DiffuseColor] = RED::Color(float(meshProps.color.m_dRed), float(meshProps.color.m_dGreen), float(meshProps.color.m_dBlue), 1.f);
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
}