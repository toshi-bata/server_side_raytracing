#include "HCLuminateBridge.h"
#include "ConversionTools.h"

#include <cmath>
#include <algorithm>

#include <REDILicense.h>
#include <REDFactory.h>
#include <REDIOptions.h>
#include <REDIResourceManager.h>
#include <REDIViewpoint.h>
#include <REDIViewpointRenderList.h>

#include <REDLayerSet.h>
#include <REDITransformShape.h>
#include <REDIShape.h>
#include <REDILightShape.h>
#include <REDFrameStatistics.h>
#include "REDImageTools.h"

#include "LuminateRCTest.h"

namespace HC_luminate_bridge {

    HCLuminateBridge::HCLuminateBridge() :
        m_frameIsComplete(false), m_newFrameIsRequired(true), m_frameTracingMode(RED::FTF_PATH_TRACING)
    {

    }

    HCLuminateBridge::~HCLuminateBridge()
    {
    }

    bool HCLuminateBridge::initialize(std::string const& a_license, void* a_osHandle, int a_windowWidth, int a_windowHeight, CameraInfo a_cameraInfo)
    {
        //////////////////////////////////////////
        // Assign Luminate license.
        //////////////////////////////////////////

        bool licenseIsActive;
        RED_RC rc = setLicense(a_license.c_str(), licenseIsActive);
        if (rc != RED_OK || !licenseIsActive)
            return false;
        //////////////////////////////////////////
        // Select Luminate rendering mode.
        //////////////////////////////////////////

        rc = setSoftTracerMode(1);
        if (rc != RED_OK)
            return false;

        //////////////////////////////////////////
        // Retrieve the resource manager from singleton
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Set global rendering options.
        //////////////////////////////////////////

        RED::IOptions* ioptions = resourceManager->As<RED::IOptions>();
        RC_CHECK(ioptions->SetOptionValue(RED::OPTIONS_RAY_USE_EMBREE, true, iresourceManager->GetState()));

        // Enable raytracer.
        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_RAY_PRIMARY, true, iresourceManager->GetState()));

        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_TONE_MAPPING_IGNORE_BACKGROUND, false, iresourceManager->GetState()));

        // Set raytracing only options
        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_RAY_GI, true, iresourceManager->GetState()));
        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_RAY_GI_CACHE_PASSES_COUNT, 3, iresourceManager->GetState()));
        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_RAY_SHADOWS, 3, iresourceManager->GetState()));
        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_RAY_REFLECTIONS, 3, iresourceManager->GetState()));
        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_RAY_REFRACTIONS, 3, iresourceManager->GetState()));
        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_RAY_TRANSPARENCY, 3, iresourceManager->GetState()));

        // Set pathtracing only options
        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_PATH_GI, 3, iresourceManager->GetState()));

        // We have the main thread and Vis uses some threads too.
        // The main thread is preserved by Luminate itself.
        // Limit the number of threads used by the soft tracer to preserve some interactivity.
        int coreCount = iresourceManager->GetNumberOfProcessors();
        //int rayMaxThreadCount = std::max(1, coreCount - 2);
        int rayMaxThreadCount = coreCount - 2;
        RC_CHECK(ioptions->SetOptionValue(RED::OPTIONS_RAY_MAX_THREADS, rayMaxThreadCount, iresourceManager->GetState()));

        //////////////////////////////////////////
        // Create a Luminate window.
        //////////////////////////////////////////
        m_windowWidth = a_windowWidth;
        m_windowHeight = a_windowHeight;

#ifdef _LIN32
        rc = createRedWindow(a_osHandle, m_windowWidth, m_windowHeight, a_display, a_screen, a_visual, m_window);
#else
        rc = createRedWindow(a_osHandle, m_windowWidth, m_windowHeight, m_window);
#endif

        if (rc != RED_OK)
            return false;

        //////////////////////////////////////////
        // Set window/VRL scope rendering options
        //////////////////////////////////////////

        // Turn on HDR
        RC_CHECK(ioptions->SetOptionValue(RED::OPTIONS_WINDOW_HDR, 1, iresourceManager->GetState()));

        // Set number of ray sent for each pixel.

        RED::IWindow* iwindow = m_window->As<RED::IWindow>();

        // Auxiliary VRL creation
        rc = iwindow->CreateVRL(m_auxvrl, m_windowWidth, m_windowHeight, RED::FMT_RGBA, true, iresourceManager->GetState());

        RED::IViewpointRenderList* iauxvrl = m_auxvrl->As<RED::IViewpointRenderList>();
        RC_CHECK(iauxvrl->SetSoftAntiAlias(20, iresourceManager->GetState()));

        //////////////////////////////////////////
        // Create and initialize Luminate camera.
        //////////////////////////////////////////
        rc = createCamera(m_window, m_windowWidth, m_windowHeight, 1, m_auxcamera);
        if (rc != RED_OK)
            return false;

        RC_TEST(iauxvrl->SetClearColor(RED::Color::WHITE, iresourceManager->GetState()));

        //////////////////////////////////////////
        // Synchronize HC and Luminate cameras for
        // the first time.
        //////////////////////////////////////////

        rc = syncLuminateCamera(a_cameraInfo);
        if (rc != RED_OK)
            return false;

        //////////////////////////////////////////
        // Simulate an empty scene.
        // This is required to attach lighting env
        // on the scene root.
        //////////////////////////////////////////
        m_conversionDataPtr.reset(new LuminateSceneInfo());
        m_conversionDataPtr->rootTransformShape = RED::Factory::CreateInstance(CID_REDTransformShape);
        addSceneToCamera(m_auxcamera, *m_conversionDataPtr);

        //////////////////////////////////////////
        // Initialize the lighting environment with
        // physical sun/sky or a background image.
        // If the scene is initialy empty, we do not
        // need to add it anywhere.
        //////////////////////////////////////////
        //rc = createDefaultModel(m_defaultLightingModel);

        return true;
    }

    FrameStatistics HCLuminateBridge::getFrameStatistics() { return m_lastFrameStatistics; }

    bool HCLuminateBridge::shutdown()
    {
        //////////////////////////////////////////
        // Shutdown completely Luminate.
        // This will destroy absolutely all, including
        // shared resources.
        //////////////////////////////////////////

        return shutdownLuminate(m_window) == RED_OK;
    }

    bool HCLuminateBridge::resize(int a_windowWidth, int a_windowHeight)
    {
        //////////////////////////////////////////
        // Resize Luminate window.
        //////////////////////////////////////////

        RED_RC rc = resizeWindow(m_window, a_windowWidth, a_windowHeight);
        if (rc != RED_OK)
            return false;

        m_windowWidth = a_windowWidth;
        m_windowHeight = a_windowHeight;

        //////////////////////////////////////////
        // As window size has changed, a camera sync
        // is required for projection computation.
        //////////////////////////////////////////

        // But don't do it if width or height is == 0
        if (m_windowHeight == 0 || m_windowWidth == 0)
            return true;

        //return syncLuminateCamera() == RED_OK;
        return true;
    }

    bool HCLuminateBridge::draw()
    {
        // update segments if necessary
        //if (m_selectedSegmentTransformIsDirty) {
        //    updateSelectedSegmentTransform();
        //}

        // update root transform if necessary
        //if (m_rootTransformIsDirty) {
        //    syncRootTransform();
        //}

        //if (m_bSyncCamera)
        //    checkCameraSync();

        RED_RC rc = checkDrawTracing(m_window, m_frameTracingMode, m_frameIsComplete, m_newFrameIsRequired);
        //RED_RC rc = checkDrawHardware(m_window);

        checkFrameStatistics(m_window, &m_lastFrameStatistics, m_frameIsComplete);

        return rc == RED_OK;
    }

    bool HCLuminateBridge::syncScene(std::vector <MeshPropaties> aMeshProps, CameraInfo a_cameraInfo)
    {
        //////////////////////////////////////////
        // Retrieve the resource manager from singleton
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Interrupt all ongoing frame tracing thread
        // operations.
        //////////////////////////////////////////

        RED::IWindow* window = m_window->As<RED::IWindow>();
        window->FrameTracingStop();

        //////////////////////////////////////////
        // Convert 3DF current scene to Luminate scene
        // and add it to a new camera.
        // Starting from a new camera allow to
        // restart with a fresh acceleration structure
        // and avoid perf issues if old and new scenes
        // have huge scale differences.
        //////////////////////////////////////////

        RED::Object* newCamera = nullptr;
        RED_RC rc = createCamera(m_window, m_windowWidth, m_windowHeight, 1, newCamera);
        if (rc != RED_OK)
            return false;

        std::shared_ptr<LuminateSceneInfo> newSceneInfo = convertScene(aMeshProps);
        addSceneToCamera(newCamera, *newSceneInfo);
        
        //////////////////////////////////////////
        // Add lighting environment to the scene
        // root.
        //////////////////////////////////////////

        //switch (m_lightingModel) {
        //case LightingModel::Default:
        //    addDefaultModel(m_window, newSceneInfo->rootTransformShape, m_defaultLightingModel);
        //    break;
        //case LightingModel::PhysicalSunSky:
        //    addSunSkyModel(m_window, newSceneInfo->rootTransformShape, m_sunSkyLightingModel);
        //    break;
        //case LightingModel::EnvironmentMap:
        //    addEnvironmentMapModel(m_window, newSceneInfo->rootTransformShape, m_environmentMapLightingModel);
        //    break;
        //};

        //////////////////////////////////////////
        // Remove and cleanup previous scene and
        // replace current camera by the new one.
        //////////////////////////////////////////
        
        if (m_conversionDataPtr != nullptr) {
            rc = destroyScene(*m_conversionDataPtr);
            m_conversionDataPtr.reset();
            if (rc != RED_OK)
                return false;
        }

        rc = RED::Factory::DeleteInstance(m_auxcamera, iresourceManager->GetState());
        if (rc != RED_OK)
            return false;

        m_conversionDataPtr = newSceneInfo;

        //////////////////////////////////////////
        // Synchronise the new camera with the
        // new scene.
        //////////////////////////////////////////

        m_auxcamera = newCamera;
        
        return syncLuminateCamera(a_cameraInfo) == RED_OK;
        
        return true;
    }

    CameraInfo HCLuminateBridge::creteCameraInfo(double* a_target, double* a_up, double* a_position, 
        int a_projection, double a_width, double a_height)
    {
        //////////////////////////////////////////
        // Compute camera axis in world space.
        //////////////////////////////////////////

        RED::Vector3 eyePosition, right, top, sight, target;

        target = RED::Vector3(a_target[0], a_target[1], a_target[2]);
        top = RED::Vector3(a_up[0], a_up[1], a_up[2]);
        eyePosition = RED::Vector3(a_position[0], a_position[1], a_position[2]);

        sight = target - eyePosition;
        sight.Normalize();

        right = top.Cross2(sight);
        right.Normalize();

        //////////////////////////////////////////
        // Get camera projection mode
        //////////////////////////////////////////

        ProjectionMode projectionMode = ProjectionMode::Perspective;

        //////////////////////////////////////////
        // Return result.
        //////////////////////////////////////////

        CameraInfo cameraInfo;
        cameraInfo.projectionMode = projectionMode;
        cameraInfo.target = target;
        cameraInfo.top = top;
        cameraInfo.eyePosition = eyePosition;
        cameraInfo.sight = sight;
        cameraInfo.right = right;
        cameraInfo.field_width = a_width;
        cameraInfo.field_height = a_height;

        return cameraInfo;
    }

    bool HCLuminateBridge::saveImg()
    {
        RED_RC rc;

        RED::IViewpointRenderList* defaultVRL = m_auxvrl->As<RED::IViewpointRenderList>();
        RED::Object* renderimg = defaultVRL->GetRenderImage();

        rc = RED::ImageTools::Save(renderimg, false, "C:\\temp\\my_image.png", false, true, 1.0);

        return true;
    }

    RED_RC HCLuminateBridge::syncLuminateCamera(CameraInfo a_cameraInfo)
    {
        Handedness viewHandedness =
            m_conversionDataPtr != nullptr ? m_conversionDataPtr->viewHandedness : Handedness::RightHanded;
        RED_RC rc = syncCameras(m_auxcamera, viewHandedness, m_windowWidth, m_windowHeight, a_cameraInfo);

        if (rc != RED_OK)
            return rc;

        //rc = synchronizeAxisTriadWithCamera(m_axisTriad, m_camera);
        m_newFrameIsRequired = true;

        return rc;
    }

    RED_RC
        HCLuminateBridge::createCamera(RED::Object* a_window, int a_windowWidh, int a_windowHeight, int a_vrlId, RED::Object*& a_outCamera)
    {
        //////////////////////////////////////////
        // Create a Luminate camera which will render
        // in the window.
        //////////////////////////////////////////

        RC_TEST(createRedCamera(a_window, a_windowWidh, a_windowHeight, a_vrlId, a_outCamera));

        //////////////////////////////////////////
        // Set camera scope rendering options
        //////////////////////////////////////////

        RED::IViewpoint* iviewpoint = a_outCamera->As<RED::IViewpoint>();

        // Turn on tone mapping.
        RED::PostProcess& pp = iviewpoint->GetPostProcessSettings();
        RC_TEST(pp.SetToneMapping(RED::TMO_PHOTOGRAPHIC));

        return RED_OK;
    }

    LuminateSceneInfoPtr HCLuminateBridge::convertScene(std::vector <MeshPropaties> aMeshProps)
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
        RED::ITransformShape* itransform = rootTransformShape->As<RED::ITransformShape>();

        //////////////////////////////////////////
        // Manage scene handedness.
        // Luminate is working with a right-handed coordinate system,
        // thus if the input scene is left-handed we must apply
        // a transformation on the conversion root to go right-handed.
        //////////////////////////////////////////

        //Handedness viewHandedness = getViewHandedness(a_hpsView);
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

        ConversionContextHPSPtr sceneInfoPtr = std::make_shared<ConversionContextHPS>();
        sceneInfoPtr->rootTransformShape = rootTransformShape;
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

    RED::Object* convertNordTree(RED::Object* a_resourceManager, ConversionContextHPS& a_ioConversionContext, MeshPropaties meshProps)
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

        iresourceManager->CreateMaterial(material, iresourceManager->GetState());
        RED::IMaterial* imatr = material->As< RED::IMaterial >();
        RED::Color color = RED::Color(float(meshProps.color.m_dRed), float(meshProps.color.m_dGreen), float(meshProps.color.m_dBlue), 1.f);
        rc = imatr->SetupGenericDiffuseMaterial(false, color, NULL, RED::Matrix::IDENTITY, RED::MCL_TEX0,
            &RED::LayerSet::ALL_LAYERS, NULL, a_resourceManager, iresourceManager->GetState());

        RED::Object* shape = nullptr;
        shape = convertHEMeshToREDMeshShape(iresourceManager->GetState(), meshProps.meshData);

        if (shape != nullptr)
            meshShapes.push_back(shape);

        //////////////////////////////////////////
        // Create RED transform shape associated to segment
        //////////////////////////////////////////

        RED::Object* transform = RED::Factory::CreateInstance(CID_REDTransformShape);
        RED::ITransformShape* itransform = transform->As<RED::ITransformShape>();

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

    RED::Object* convertHEMeshToREDMeshShape(const RED::State& a_state, A3DMeshData a_meshData)
    {
        RED_RC rc;

        RED::Object* result = RED::Factory::CreateInstance(CID_REDMeshShape);
        RED::IMeshShape* imesh = result->As<RED::IMeshShape>();

        std::vector<float> points;
        for (int i = 0; i < a_meshData.m_uiCoordSize; i++)
            points.push_back(a_meshData.m_pdCoords[i]);

        rc = imesh->SetArray(RED::MCL_VERTEX, points.data(), a_meshData.m_uiCoordSize / 3, 3, RED::MFT_FLOAT, a_state);

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

        return result;
    }

    RED_RC resizeWindow(RED::Object* a_window, int a_newWidth, int a_newHeight)
    {
        // Disable resize if newWidth or newHeight == 0
        if (a_newWidth == 0 || a_newHeight == 0)
            return RED_OK;

        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Resize the window.
        //////////////////////////////////////////

        RED::IWindow* iwindow = a_window->As<RED::IWindow>();
        RC_TEST(iwindow->Resize(a_newWidth, a_newHeight, iresourceManager->GetState()));

        return RED_OK;
    }

    RED_RC addSceneToCamera(RED::Object* a_camera, LuminateSceneInfo const& a_sceneInfo)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Add the scene to the camera.
        //////////////////////////////////////////

        RED::IViewpoint* iviewpoint = a_camera->As<RED::IViewpoint>();
        RC_TEST(iviewpoint->AddShape(a_sceneInfo.rootTransformShape, iresourceManager->GetState()));

        return RED_OK;
    }

    RED_RC setLicense(char const* a_license, bool& a_outLicenseIsActive)
    {
        a_outLicenseIsActive = false;

        //////////////////////////////////////////
        // Get the resource manager singleton
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);

        //////////////////////////////////////////
        // Set the license and check its validity
        //////////////////////////////////////////

        RED::ILicense* ilicense = resourceManager->As<RED::ILicense>();
        RC_TEST(ilicense->SetLicense(a_license));
        RC_TEST(ilicense->IsProductActivated(a_outLicenseIsActive, RED::PROD_REDSDK));

        return RED_OK;
    }

    RED_RC setSoftTracerMode(int a_softTracerMode)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Set the soft tracer mode
        //////////////////////////////////////////

        RED::IOptions* ioptions = resourceManager->As<RED::IOptions>();
        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_RAY_ENABLE_SOFT_TRACER, a_softTracerMode, iresourceManager->GetState()));

        return RED_OK;
    }

    RED_RC createRedWindow(void* a_osHandler,
        int a_width,
        int a_height,
#ifdef _LIN32
        Display* a_display,
        int a_screen,
        Visual* a_visual,
#endif
        RED::Object*& a_outWindow)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);

        //////////////////////////////////////////
        // Create the new Luminate window
        //////////////////////////////////////////

        RED_RC rc;

#if _LIN32
        a_outWindow = RED::Factory::CreateREDWindow(
            *resourceManager, a_osHandler, a_display, a_screen, a_visual, a_width, a_height, nullptr, rc);
#else
        a_outWindow = RED::Factory::CreateREDWindow(*resourceManager, a_osHandler, a_width, a_height, nullptr, rc);
#endif

        return rc;
    }

    RED_RC shutdownLuminate(RED::Object* a_window)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Stop all tracing threads.
        //////////////////////////////////////////

        RED::IWindow* window = a_window->As<RED::IWindow>();
        window->FrameTracingStop();

        //////////////////////////////////////////
        // Destroy the resource manager.
        //////////////////////////////////////////

        RC_TEST(RED::Factory::DeleteInstance(resourceManager, iresourceManager->GetState()));

        return RED_OK;
    }

    RED_RC createRedCamera(RED::Object* a_window, int a_windowWidth, int a_windowHeight, int a_vrlId, RED::Object*& a_outCamera)
    {
        a_outCamera = nullptr;

        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Create a new full-window viewpoint and add
        // it to window's viewpoint render list.
        //
        // The default InsertViewpoint call sets VSP_SIZE_STRETCHED_AUTO_RATIO, which computes ratio differently from 3DF/HPS.
        // Here we set VSP_SIZE_STRETCHED to handle the ratio ourselves.
        //////////////////////////////////////////

        RED::Object* viewpoint = RED::Factory::CreateInstance(CID_REDViewpoint);
        RED::IWindow* iwindow = a_window->As<RED::IWindow>();

        RED::Object* defaultVRLObj = NULL;
        RC_TEST(iwindow->GetVRL(defaultVRLObj, a_vrlId));

        RED::IViewpointRenderList* defaultVRL = defaultVRLObj->As<RED::IViewpointRenderList>();

        RC_TEST(defaultVRL->InsertViewpoint(viewpoint,
            RED::VST_SCENE,
            RED::LIST_FIRST,
            0,
            0,
            a_windowWidth,
            a_windowHeight,
            0.0f,
            0.0f,
            RED::VSP_ANCHOR_STRETCHED,
            RED::VSP_SIZE_STRETCHED,
            iresourceManager->GetState()));

        a_outCamera = viewpoint;
        return RED_OK;
    }

    RED_RC destroyScene(LuminateSceneInfo const& a_sceneInfo)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Delete scene root and all scene shared resources.
        //////////////////////////////////////////

        RC_TEST(RED::Factory::DeleteInstance(a_sceneInfo.rootTransformShape, iresourceManager->GetState()));

        for (auto const& materialEntry : a_sceneInfo.materials) {
            RC_TEST(iresourceManager->DeleteMaterial(materialEntry.second, iresourceManager->GetState()));
        }

        for (auto const& imageEntry : a_sceneInfo.imageNameToLuminateMap) {
            RC_TEST(iresourceManager->DeleteImage(imageEntry.second, iresourceManager->GetState()));
        }

        return RED_OK;
    }

    RED_RC syncCameras(RED::Object* a_camera,
        Handedness a_sceneHandedness,
        int a_windowWidth,
        int a_windowHeight,
        CameraInfo const& a_cameraInfo)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        //
        //
        // Luminate needs its ViewingAxis to be right-handed (opengl convention).
        // But if 3DF worldViewAxis is left-handed, from the 3DF axis :
        // - we can't flip the 'sight' axis, as we'd have a camera looking at the opposite direction of 3DF
        // - we can't flip the 'right' axis, as we'd have the rendering mirrored left-to-right
        //
        // To keep the 3 axis orientation the same between our 3DF left-handed and Luminate right-handed, we need
        // to exchange the Y and Z axis.
        //
        // This axis exchange is done in *two* locations using a transformation matrix :
        // 1. here, to construct the camera
        // 2. in the Luminate model root transform
        //
        // == 3DF camera ================           == Luminate camera ================
        //
        //  [Y up]                                   [Z up]
        //    ^                                        ^
        //    |  7 [Z sight]                           |  7 [Y sight]
        //    | /                                      | /
        //    |/                                       |/
        //    +-----------> [X]                        +-----------> [X]
        //
        //////////////////////////////////////////

        RED::Vector3 eyePosition = a_cameraInfo.eyePosition;
        RED::Vector3 right = a_cameraInfo.right;
        RED::Vector3 top = a_cameraInfo.top;
        RED::Vector3 sight = a_cameraInfo.sight;
        RED::Vector3 target = a_cameraInfo.target;

        //if (a_sceneHandedness == Handedness::LeftHanded) {
        //    eyePosition = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * a_cameraInfo.eyePosition;
        //    sight = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * a_cameraInfo.sight;
        //    top = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * a_cameraInfo.top;
        //    right = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * a_cameraInfo.right;
        //    target = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * a_cameraInfo.target;
        //}
        //else if (a_sceneHandedness == Handedness::RightHanded) {
            // Even with 3DF/HPS right-handed we still need to flip the x axis.
            // For some reasons, 3DF seems to add a - on x axis when computing the view matrix.
            // To get similar result in RED we must mimic it.
            right = -right;
        //}

        sight.Normalize();
        top.Normalize();
        right.Normalize();

        RED::IViewpoint* iviewpoint = a_camera->As<RED::IViewpoint>();
        RC_CHECK(iviewpoint->SetEye(eyePosition, iresourceManager->GetState()));
        RC_CHECK(iviewpoint->SetViewingAxis(sight, top, right, iresourceManager->GetState()));

        double const distanceToTarget = (target - eyePosition).GetLength();
        // In 3DF znear = camera near limit * distance to target, with near limit [0.001, 0.85]. As it is not useful
        // to sync this part with Luminate, let's keep the min value.
        double const MIN_3DF_NEAR_LIMIT = 0.001;
        double znear = MIN_3DF_NEAR_LIMIT * distanceToTarget;

        // In 3DF there is no zfar implementation, it is considered as infinite which is simplify terms using it
        // and has very little influence on final Z mapping.
        // As we need a numerical value in Luminate, lets take it big enough to ensure to always see full model.
        double zfar = 1000.0 * distanceToTarget;

        // Apply znear and zfar
        RC_CHECK(iviewpoint->SetNearFar(znear, zfar, iresourceManager->GetState()));

        // NOTE: Luminate considers aspect ratio == (height / width)
        double luminateAspectRatio = (double)a_windowHeight / (double)a_windowWidth;
        double cameraFieldAspectRatio = (double)a_cameraInfo.field_height / (double)a_cameraInfo.field_width;

        // 3DF camera field width/height describe the size of the view rectangle
        // on the plane passing through the target point and parallel to camera near plane.
        double fieldHeight = (double)a_cameraInfo.field_height;
        double fieldWidth = (double)a_cameraInfo.field_width;

        // Reproduce the 3DGS 'auto ratio' behavior
        if (cameraFieldAspectRatio > luminateAspectRatio) {
            fieldWidth = fieldHeight / luminateAspectRatio;
        }

        // Projection

        if (a_cameraInfo.projectionMode == ProjectionMode::Perspective) {
            // Compute the effective camera fovy ( half angle opening on X axis with Luminate )
            double halfFOVXAngleInRadians = std::atan2(0.5 * fieldWidth, distanceToTarget);
            RC_CHECK(
                iviewpoint->SetFrustumPerspective(halfFOVXAngleInRadians, luminateAspectRatio, iresourceManager->GetState()));
        }
        else if (a_cameraInfo.projectionMode == ProjectionMode::Orthographic) {
            // Compute the ortho volume size according red window ratio.

            double new_w = a_cameraInfo.field_width;
            double new_h = a_cameraInfo.field_height;

            if (a_windowWidth > a_windowHeight)
                new_w = a_cameraInfo.field_height / luminateAspectRatio;
            else if (a_windowHeight > a_windowWidth)
                new_h = a_cameraInfo.field_width * luminateAspectRatio;

            // Set ortho projection.
            // The first two parameters are the half size of the ortho volume.
            RC_CHECK(iviewpoint->SetFrustumParallel(new_w / 2, new_h / 2, iresourceManager->GetState()));
        }
        else if (a_cameraInfo.projectionMode == ProjectionMode::Stretched) {
            // TODO
        }

        return RED_OK;
    }

    RED_RC checkDrawTracing(RED::Object* a_window,
        RED::FRAME_TRACING_FEEDBACK a_tracingMode,
        bool& a_ioFrameIsComplete,
        bool& a_ioNewFrameIsRequired)
    {
        /*
         * This method should be called constantly and will check by itself if a render is required.
         *
         * A render is required in 2 cases :
         * 1) A new frame has been requested
         * 2) The current frame is not fully raytraced
         *
         * A new frame is required in some cases :
         * 1) A new model has been loaded
         * 2) The camera has moved
         * 3) The window has been resized
         *
         */

         //////////////////////////////////////////
         // Get the resource manager singleton.
         //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        RED::IWindow* iwindow = a_window->As<RED::IWindow>();

        //////////////////////////////////////////
        // Submit the current transaction if a new
        // frame start is required.
        // While the same image is being refined
        // the same transaction must be kept.
        // Otherwise a new transaction must be open.
        //////////////////////////////////////////

        if (a_ioNewFrameIsRequired) {
            RC_TEST(iresourceManager->EndState());
            iresourceManager->BeginState();
            a_ioNewFrameIsRequired = false;
            a_ioFrameIsComplete = false;
        }

        //////////////////////////////////////////
        // Refine current image while not completed.
        //////////////////////////////////////////

        if (!a_ioFrameIsComplete) {
            // Note: The feedback interval impact only the main thread, making it spending plus or less
            // time computing stuff before hand over to the caller.
            // For interactivity purpose we want to keep it pretty low and will not impact a lot the computation
            // time on machines with many logical cores as it slows down only the main one.
            // But be aware that on machine with low number of cores, this could be more significant.
            float interval = 10.f;

            RC_TEST(iwindow->FrameTracing(a_ioFrameIsComplete, a_tracingMode, interval));
        }

        return RED_OK;
    }

    RED_RC checkDrawHardware(RED::Object* a_window)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Submit current transaction
        //////////////////////////////////////////

        RC_TEST(iresourceManager->EndState());

        //////////////////////////////////////////
        // Perform draw
        //////////////////////////////////////////

        RED::IWindow* iwindow = a_window->As<RED::IWindow>();
        RC_TEST(iwindow->FrameDrawing());

        //////////////////////////////////////////
        // Open new transaction
        //////////////////////////////////////////

        iresourceManager->BeginState();

        return RED_OK;
    }

    RED_RC checkFrameStatistics(RED::Object* a_window, FrameStatistics* a_stats, bool& a_ioFrameIsComplete)
    {
        if (!a_ioFrameIsComplete) {
            RED::IWindow* iwindow = a_window->As<RED::IWindow>();

            RED::FrameStatistics fstats = iwindow->GetFrameStatistics();
            const RED::ViewpointStatistics& camstats = fstats.GetViewpointStatistics(1, 0);

            a_stats->renderingProgress = camstats.GetSoftwarePassProgress();
            a_stats->remainingTimeMilliseconds = camstats.GetSoftwareRemainingTime();
            a_stats->numberOfPasses = camstats.GetSoftwareRenderStepPassesCount();
            a_stats->currentPass = camstats.GetSoftwareRenderStepPass();
        }
        else {
            a_stats->renderingProgress = 1.f;
            a_stats->remainingTimeMilliseconds = 0.f;
        }
        a_stats->renderingIsDone = a_ioFrameIsComplete;
        return RED_OK;
    }
}