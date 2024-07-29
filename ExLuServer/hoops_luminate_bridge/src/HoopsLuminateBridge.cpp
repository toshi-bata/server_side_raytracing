#define NOMINMAX
#include <cmath>
#include <algorithm>

#include <REDILicense.h>
#include <REDFactory.h>
#include <REDILightShape.h>
#include <REDIPhysicalLightShape.h>
#include <REDIOptions.h>
#include <REDIShape.h>
#include <REDITransformShape.h>
#include <REDIResourceManager.h>
#include <REDIWindow.h>
#include <REDIViewpoint.h>
#include <REDIViewpointRenderList.h>
#include <REDIREDFile.h>
#include <REDVector3.h>
#include <REDMatrix.h>
#include <REDFrameStatistics.h>
#include "REDImageTools.h"

#include <hoops_luminate_bridge/LuminateRCTest.h>
#include <hoops_luminate_bridge/HoopsLuminateBridge.h>
#include <hoops_luminate_bridge/LightingEnvironment.h>

#ifdef _LIN32
    #include <X11/Xlib.h>
#endif

namespace hoops_luminate_bridge {

    SelectedSegmentInfo::~SelectedSegmentInfo() {}

    const RED::Matrix HoopsLuminateBridge::s_leftHandedToRightHandedMatrix =
        RED::Matrix(RED::Vector3(1, 0, 0), RED::Vector3(0, 0, 1), RED::Vector3(0, 1, 0), RED::Vector3(0, 0, 0));

    HoopsLuminateBridge::HoopsLuminateBridge():
        m_window(nullptr), m_frameIsComplete(false), m_newFrameIsRequired(true), m_axisTriad(), m_bSyncCamera(false),
        m_lightingModel(LightingModel::No), m_windowWidth(0), m_windowHeight(0), m_defaultLightingModel(),
        m_sunSkyLightingModel(), m_environmentMapLightingModel(), m_frameTracingMode(RED::FTF_PATH_TRACING),
        m_selectedSegmentTransformIsDirty(false), m_rootTransformIsDirty(false)
    {
    }

    HoopsLuminateBridge::~HoopsLuminateBridge()
    {
        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        // clean default model
        iresourceManager->DeleteImage(m_defaultLightingModel.backgroundCubeImage, iresourceManager->GetState());
        RED::Factory::DeleteInstance(m_defaultLightingModel.skyLight, iresourceManager->GetState());

        // clean sun sky model
        iresourceManager->DeleteImage(m_sunSkyLightingModel.backgroundCubeImage, iresourceManager->GetState());
        RED::Factory::DeleteInstance(m_sunSkyLightingModel.skyLight, iresourceManager->GetState());
        RED::Factory::DeleteInstance(m_sunSkyLightingModel.sunLight, iresourceManager->GetState());

        // clean env map
        if (m_environmentMapLightingModel.imagePath != "") {
            iresourceManager->DeleteImage(m_environmentMapLightingModel.backgroundCubeImage, iresourceManager->GetState());
            RED::Factory::DeleteInstance(m_environmentMapLightingModel.skyLight, iresourceManager->GetState());
        }
    }

    bool HoopsLuminateBridge::initialize(/*std::string const& a_license,*/
                                         void* a_osHandle,
                                         int a_windowWidth,
                                         int a_windowHeight,
#ifdef _LIN32
                                         Display* a_display,
                                         int a_screen,
                                         Visual* a_visual,
#endif
                                         std::string const& a_environmentMapFilepath, CameraInfo a_cameraInfo)
    {
        RED_RC rc;
        
        //////////////////////////////////////////
        // Init 3DF/HPS specifics camera.
        //////////////////////////////////////////

        //saveCameraState();

        //////////////////////////////////////////
        // Assign Luminate license.
        //////////////////////////////////////////

        //bool licenseIsActive;
        //RED_RC rc = setLicense(a_license.c_str(), licenseIsActive);
        //if (rc != RED_OK || !licenseIsActive)
        //    return false;

        //////////////////////////////////////////
        // Select Luminate rendering mode.
        //////////////////////////////////////////

        //rc = setSoftTracerMode(1);
        //if (rc != RED_OK)
        //    return false;

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
        int rayMaxThreadCount = std::max(1, coreCount - 2);
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

        //RED::Object* defaultVRLObj = NULL;
        //RC_CHECK(iwindow->GetVRL(defaultVRLObj, 0));

        //RED::IViewpointRenderList* defaultVRL = defaultVRLObj->As<RED::IViewpointRenderList>();
        //RC_CHECK(defaultVRL->SetSoftAntiAlias(20, iresourceManager->GetState()));

        // Auxiliary VRL creation
        RED::Object* auxvrl;
        rc = iwindow->CreateVRL(auxvrl, a_windowWidth, a_windowHeight, RED::FMT_RGBA, true, iresourceManager->GetState());

        RED::IViewpointRenderList* iauxvrl = auxvrl->As<RED::IViewpointRenderList>();
        RC_CHECK(iauxvrl->SetSoftAntiAlias(20, iresourceManager->GetState()));

        //////////////////////////////////////////
        // Create and initialize Luminate camera.
        //////////////////////////////////////////

        rc = createCamera(m_window, m_windowWidth, m_windowHeight, 1, m_camera);
        if (rc != RED_OK)
            return false;

        //////////////////////////////////////////
        // Initialize an axis triad to be displayed
        //////////////////////////////////////////

        //rc = createAxisTriad(m_window, 1, m_axisTriad);
        //if (rc != RED_OK)
        //    return false;

        //////////////////////////////////////////
        // Synchronize 3DF and Luminate cameras for
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
        addSceneToCamera(m_camera, *m_conversionDataPtr);

        //////////////////////////////////////////
        // Initialize the lighting environment with
        // physical sun/sky or a background image.
        // If the scene is initialy empty, we do not
        // need to add it anywhere.
        //////////////////////////////////////////
        if (NULL == m_defaultLightingModel.backgroundCubeImage)
            rc = createDefaultModel(m_defaultLightingModel);
        if (NULL == m_sunSkyLightingModel.backgroundCubeImage)
            rc = createPhysicalSunSkyModel(m_sunSkyLightingModel);

        if (a_environmentMapFilepath.empty())
            rc = setDefaultLightEnvironment();
        else {
            rc = createEnvironmentImageLightingModel(
                a_environmentMapFilepath.c_str(), RED::Color::WHITE, true, m_environmentMapLightingModel);
            //rc = setEnvMapLightEnvironment(a_environmentMapFilepath, true, RED::Color::WHITE);
        }

        return rc == RED_OK;
    }

    void HoopsLuminateBridge::invalidateSelectedSegmentTransform() { m_selectedSegmentTransformIsDirty = true; }

    void HoopsLuminateBridge::invalidateRootTransform() { m_rootTransformIsDirty = true; }

    FrameStatistics HoopsLuminateBridge::getFrameStatistics() { return m_lastFrameStatistics; }

    std::shared_ptr<LuminateSceneInfo> HoopsLuminateBridge::getConversionData() const { return m_conversionDataPtr; }

    SelectedSegmentInfoPtr HoopsLuminateBridge::getSelectedSegmentInfo() const { return m_selectedSegment; }

    void HoopsLuminateBridge::setSelectedSegmentInfo(SelectedSegmentInfoPtr a_selectedSegmentInfo)
    {
        m_selectedSegment = a_selectedSegmentInfo;
    }

    RED::Object* HoopsLuminateBridge::getWindow() const { return m_window; }

    void HoopsLuminateBridge::setFrameTracingMode(int a_mode)
    {
        m_frameTracingMode = static_cast<RED::FRAME_TRACING_FEEDBACK>(a_mode);
    }

    void HoopsLuminateBridge::setSoftAAValue(int a_value)
    {
        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        RED::IWindow* iwindow = m_window->As<RED::IWindow>();
        RED::Object* defaultVRLObj = NULL;
        RC_CHECK(iwindow->GetVRL(defaultVRLObj, 0));

        RED::IViewpointRenderList* defaultVRL = defaultVRLObj->As<RED::IViewpointRenderList>();
        RC_CHECK(defaultVRL->SetSoftAntiAlias(a_value, iresourceManager->GetState()));
    }

    bool HoopsLuminateBridge::shutdown()
    {
        //////////////////////////////////////////
        // Shutdown completely Luminate.
        // This will destroy absolutely all, including
        // shared resources.
        //////////////////////////////////////////

        return shutdownLuminate(m_window) == RED_OK;
    }

    bool HoopsLuminateBridge::resize(int a_windowWidth, int a_windowHeight, CameraInfo a_cameraInfo)
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

        return syncLuminateCamera(a_cameraInfo) == RED_OK;
    }

    bool HoopsLuminateBridge::draw()
    {
        RED_RC rc;
        
        // update segments if necessary
        //if (m_selectedSegmentTransformIsDirty) {
        //    updateSelectedSegmentTransform();
        //}

        // update root transform if necessary
        //if (m_rootTransformIsDirty) {
        //    syncRootTransform();
        //}

        //checkCameraSync();
        if (m_bSyncCamera)
        {
            rc = syncLuminateCamera(m_cameraInfo);
            m_bSyncCamera = false;
        }

        rc = checkDrawTracing(m_window, m_frameTracingMode, m_frameIsComplete, m_newFrameIsRequired);
        // RED_RC rc = checkDrawHardware(m_window);

        checkFrameStatistics(m_window, &m_lastFrameStatistics, m_frameIsComplete);

        return rc == RED_OK;
    }

    void HoopsLuminateBridge::resetFrame()
    {
        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        if (iresourceManager->GetState().GetNumber() > 1) {
            RED::IWindow* iwin = m_window->As<RED::IWindow>();
            iwin->FrameTracingStop();

            iresourceManager->BeginState();
        }

        m_newFrameIsRequired = true;
    }

    bool HoopsLuminateBridge::syncScene(const int a_windowWidth, const int a_windowHeight, CameraInfo a_cameraInfo)
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

        std::shared_ptr<LuminateSceneInfo> newSceneInfo = convertScene();
        addSceneToCamera(newCamera, *newSceneInfo);

        //////////////////////////////////////////
        // Add lighting environment to the scene
        // root.
        //////////////////////////////////////////

        switch (m_lightingModel) {
            case LightingModel::Default:
                addDefaultModel(m_window, 1, newSceneInfo->rootTransformShape, m_defaultLightingModel);
                break;
            case LightingModel::PhysicalSunSky:
                addSunSkyModel(m_window, 1, newSceneInfo->rootTransformShape, m_sunSkyLightingModel);
                break;
            case LightingModel::EnvironmentMap:
                addEnvironmentMapModel(m_window, 1, newSceneInfo->rootTransformShape, m_environmentMapLightingModel);
                break;
        };

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

        rc = RED::Factory::DeleteInstance(m_camera, iresourceManager->GetState());
        if (rc != RED_OK)
            return false;

        m_conversionDataPtr = newSceneInfo;

        //////////////////////////////////////////
        // Synchronise the new camera with the
        // new scene.
        //////////////////////////////////////////

        m_camera = newCamera;

        return syncLuminateCamera(a_cameraInfo) == RED_OK;
    }

    bool HoopsLuminateBridge::saveAsRedFile(std::string const& a_outputFilepath)
    {
        return saveCameraAndSceneAsRedFile(m_conversionDataPtr->rootTransformShape, m_camera, a_outputFilepath) == RED_OK;
    }

    RED_RC HoopsLuminateBridge::checkCameraSync(CameraInfo a_cameraInfo)
    {
        RED_RC rc = RED_OK;

        if (checkCameraChange())
            rc = syncLuminateCamera(a_cameraInfo);

        return rc;
    }

    RED_RC HoopsLuminateBridge::syncLuminateCamera(CameraInfo a_cameraInfo)
    {
        //CameraInfo cameraInfo = getCameraInfo();

        Handedness viewHandedness =
            m_conversionDataPtr != nullptr ? m_conversionDataPtr->viewHandedness : Handedness::RightHanded;

        int windowWidth, windowHeight;

        RED::IWindow* iwindow = m_window->As<RED::IWindow>();
        RED::Object* auxvrl = NULL;
        RC_TEST(iwindow->GetVRL(auxvrl, 1));

        RED::IViewpointRenderList* iauxvrl = auxvrl->As<RED::IViewpointRenderList>();
        iauxvrl->GetSize(windowWidth, windowHeight);

        RED::Object* viewpoint;
        RC_TEST(iauxvrl->GetViewpoint(viewpoint, 0));

        RED_RC rc = syncCameras(m_camera, viewHandedness, windowWidth, windowHeight, a_cameraInfo);

        if (rc != RED_OK)
            return rc;

        //rc = synchronizeAxisTriadWithCamera(m_axisTriad, m_camera);
        m_newFrameIsRequired = true;

        return rc;
    }

    RED_RC
    HoopsLuminateBridge::createCamera(RED::Object* a_window, int a_windowWidh, int a_windowHeight, int a_vrlId, RED::Object*& a_outCamera)
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

    RED_RC HoopsLuminateBridge::removeCurrentLightingEnvironment()
    {
        RED_RC rc = RED_OK;

        if (m_conversionDataPtr == nullptr)
            return rc;

        switch (m_lightingModel) {
            case LightingModel::Default:
                rc = removeDefaultModel(m_window, 1, m_conversionDataPtr->rootTransformShape, m_defaultLightingModel);
                break;
            case LightingModel::PhysicalSunSky:
                rc = removeSunSkyModel(m_window, 1, m_conversionDataPtr->rootTransformShape, m_sunSkyLightingModel);
                break;
            case LightingModel::EnvironmentMap:
                rc = removeEnvironmentMapModel(m_window, 1, m_conversionDataPtr->rootTransformShape, m_environmentMapLightingModel);
                break;
            default:
                break;
        }

        if (rc == RED_OK)
            m_lightingModel = LightingModel::No;

        return rc;
    }

    RED_RC HoopsLuminateBridge::setDefaultLightEnvironment()
    {
        removeCurrentLightingEnvironment();
        m_lightingModel = LightingModel::Default;
        //addDefaultModel(m_window, m_conversionDataPtr->rootTransformShape, m_defaultLightingModel);
        resetFrame();

        return RED_RC();
    }

    RED_RC HoopsLuminateBridge::setSunSkyLightEnvironment()
    {
        removeCurrentLightingEnvironment();

        m_lightingModel = LightingModel::PhysicalSunSky;
        addSunSkyModel(m_window, 1, m_conversionDataPtr->rootTransformShape, m_sunSkyLightingModel);
        resetFrame();

        return RED_OK;
    }

    RED_RC HoopsLuminateBridge::setEnvMapLightEnvironment(EnvironmentMapLightingModel envMap)
    {
        removeCurrentLightingEnvironment();

        m_lightingModel = LightingModel::EnvironmentMap;

        addEnvironmentMapModel(m_window, 1, m_conversionDataPtr->rootTransformShape, envMap);
        resetFrame();

        m_environmentMapLightingModel = envMap;

        return RED_OK;
    }

    RED::Vector3 REDMat2Euler(RED::Matrix a_matrix)
    {
        RED::Vector3 angles;
        if (a_matrix._mat[0][2] == 1 || a_matrix._mat[0][2] == -1) {
            angles[2] = 0.0;
            double dlta = atan2(a_matrix._mat[0][1], a_matrix._mat[0][2]);
            if (a_matrix._mat[0][2] == -1.0) {
                angles[1] = RED_PI / 2.0;
                angles[0] = dlta;
            }
            else {
                angles[1] = -RED_PI / 2.0;
                angles[0] = dlta;
            }
        }
        else {
            angles[1] = -asin(a_matrix._mat[0][2]);
            angles[0] = atan2(a_matrix._mat[1][2] / cos(angles[1]), a_matrix._mat[2][2] / cos(angles[1]));
            angles[2] = atan2(a_matrix._mat[0][1] / cos(angles[1]), a_matrix._mat[0][0] / cos(angles[1]));
        }

        // convert from radians to degrees
        angles[0] *= 180.0 / RED_PI;
        angles[1] *= 180.0 / RED_PI;
        angles[2] *= 180.0 / RED_PI;
        return angles;
    }

    RED_RC HoopsLuminateBridge::createLight(LuminateLight* a_luminateLight, bool a_useCameraPosition)
    {
        float unitScale = 0.001f, defaultSize = 1.0f;

        RED::Object* resmgr = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresmgr = resmgr->As<RED::IResourceManager>();

        // Create the light shape:
        RED::Object* light = RED::Factory::CreateInstance(CID_REDLightShape);

        // Create the light geometry
        RED::Object* areaLight = RED::Factory::CreateInstance(CID_REDMeshShape);
        RED::IMeshShape* iareaLight = areaLight->As<RED::IMeshShape>();
        switch (a_luminateLight->shape) {
            case LightShape::Plane:
                iareaLight->Quad(RED::Vector3(0.0), defaultSize, defaultSize, iresmgr->GetState());
                break;
            case LightShape::Box:
                iareaLight->Box(RED::Vector3(0.0), RED::Vector3(defaultSize), iresmgr->GetState());
                break;
            case LightShape::Sphere:
            default:
                iareaLight->Sphere(RED::Vector3(0.0), defaultSize, 10, 10, iresmgr->GetState());
                break;
        }

        // Link the emitter to the mesh.
        RED::IPhysicalLightShape* iphy = light->As<RED::IPhysicalLightShape>();
        iphy->SetEmitter(areaLight, unitScale, iresmgr->GetState());
        // Set a luminate light.
        iphy->SetLuminousFlux((float)a_luminateLight->intensity, iresmgr->GetState());
        // Set the emission color.
        iphy->SetColor(a_luminateLight->color, iresmgr->GetState());
        // Rectangular physical lights do not require that much samples.
        iphy->SetSamplesCount(64, iresmgr->GetState());

        // add light to the scene
        RED::Object* trans = RED::Factory::CreateInstance(CID_REDTransformShape);
        RED::ITransformShape* lightTrans = trans->As<RED::ITransformShape>();
        lightTrans->AddChild(areaLight, RED_SHP_DAG_NO_UPDATE, iresmgr->GetState());
        lightTrans->AddChild(light, RED_SHP_DAG_NO_UPDATE, iresmgr->GetState());

        // set light position and orientation
        RED::Matrix lightTransfoMatrix;
        if (a_useCameraPosition) {
            RED::IViewpoint* iviewpoint = m_camera->As<RED::IViewpoint>();
            iviewpoint->GetViewingMatrix(lightTransfoMatrix);
            RED::Matrix rotationMatrix;
            rotationMatrix.RotationAngleMatrix(RED::Vector3(), RED_PI, 0.0, 0.0);
            lightTransfoMatrix = lightTransfoMatrix * rotationMatrix;
        }
        else {
            lightTransfoMatrix.RotationAngleMatrix(RED::Vector3(0),
                                                   a_luminateLight->rotation.X() * RED_PI / 180.0f,
                                                   a_luminateLight->rotation.Y() * RED_PI / 180.0f,
                                                   a_luminateLight->rotation.Z() * RED_PI / 180.0f);
            lightTransfoMatrix.SetTranslation(a_luminateLight->position);
        }
        lightTransfoMatrix.Scale(a_luminateLight->size);
        lightTrans->SetMatrix(&lightTransfoMatrix, iresmgr->GetState());
        m_camera->As<RED::IViewpoint>()->AddShape(trans, iresmgr->GetState());

        // set light material
        RED::Object* mat_quad;
        iphy->GetEmitterMaterial(mat_quad, iresmgr->GetState());
        RED::IShape* ishape = areaLight->As<RED::IShape>();
        ishape->SetMaterial(mat_quad, iresmgr->GetState());

        // activate shadows
        RED::ILightShape* ilit = light->As<RED::ILightShape>();
        ilit->SetRenderMode(RED::RM_SHADOW_CASTER, 1, iresmgr->GetState());

        a_luminateLight->light = light;
        a_luminateLight->lightTransformation = trans;
        a_luminateLight->position = lightTransfoMatrix.GetTranslation();
        a_luminateLight->size = lightTransfoMatrix.AxisScaling();
        a_luminateLight->rotation = REDMat2Euler(lightTransfoMatrix);

        return RED_RC();
    }

    RED_RC HoopsLuminateBridge::removeLight(LuminateLight* a_luminateLight)
    {
        RED::Object* resmgr = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresmgr = resmgr->As<RED::IResourceManager>();
        m_camera->As<RED::IViewpoint>()->RemoveShape(a_luminateLight->lightTransformation, iresmgr->GetState());

        RED::Factory::DeleteInstance(a_luminateLight->light, iresmgr->GetState());
        a_luminateLight->light = nullptr;
        RED::Factory::DeleteInstance(a_luminateLight->lightTransformation, iresmgr->GetState());
        a_luminateLight->lightTransformation = nullptr;

        return RED_OK;
    }

    RED_RC HoopsLuminateBridge::createEnvMapLightEnvironment(std::string const& a_imageFilepath, bool a_showImage, RED::Color const& a_backgroundColor, const char* thumbFilePath, EnvironmentMapLightingModel& envMap)
    {
        removeCurrentLightingEnvironment();

        m_lightingModel = LightingModel::EnvironmentMap;

        RED_RC rc = RED_OK;

        if (envMap.imagePath != a_imageFilepath.c_str() ||
            envMap.backColor != a_backgroundColor ||
            envMap.imageIsVisible != a_showImage)
            rc = createEnvironmentImageLightingModel(
                a_imageFilepath.c_str(), a_backgroundColor, a_showImage, envMap);

        addEnvironmentMapModel(m_window, 1, m_conversionDataPtr->rootTransformShape, envMap);

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        // Create auxiliary VRL for thumbnail
        RED::IWindow* iwindow = m_window->As<RED::IWindow>();
        RED::Object* auxvrl;
        RC_TEST(iwindow->CreateVRL(auxvrl, 320, 320, RED::FMT_RGBA, true, iresourceManager->GetState()));

        // Create cameta into the auxiliary VRL
        RED::Object* newCamera = nullptr;
        RC_TEST(createCamera(m_window, 320, 320, 2, newCamera));

        // Apply emvironment map into the auxiliary VRL
        addEnvironmentMapModel(m_window, 2, m_conversionDataPtr->rootTransformShape, envMap);

        // Draw
        for (int i = 0; i < 10; i++)
            draw();

        // Save thumbnail image
        RED::IViewpointRenderList* iauxvrl = auxvrl->As<RED::IViewpointRenderList>();
        RED::Object* renderimg = iauxvrl->GetRenderImage();

        RC_TEST(RED::ImageTools::Save(renderimg, false, thumbFilePath, false, true, 1.0));

        // Delete auxiliary VRL
        RC_TEST(iwindow->DeleteVRL(auxvrl, iresourceManager->GetState()));

        resetFrame();

        m_environmentMapLightingModel = envMap;

        return rc;
    }

    CameraInfo HoopsLuminateBridge::creteCameraInfo(double* a_target, double* a_up, double* a_position,
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

        ProjectionMode projectionMode = ProjectionMode::Orthographic;

        if (0 == a_projection)
            projectionMode = ProjectionMode::Perspective;

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

    bool HoopsLuminateBridge::saveImg(const char* filePath)
    {
        RED::IWindow* iwindow = m_window->As<RED::IWindow>();

        RED::Object* auxvrl = NULL;
        RC_TEST(iwindow->GetVRL(auxvrl, 1));

        RED::IViewpointRenderList* iauxvrl = auxvrl->As<RED::IViewpointRenderList>();
        RED::Object* renderimg = iauxvrl->GetRenderImage();

        RC_TEST(RED::ImageTools::Save(renderimg, false, filePath, false, true, 1.0));
        return true;
    }

    RED_RC HoopsLuminateBridge::syncModelTransform(double* matrix)
    {
        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();
        LuminateSceneInfoPtr sceneInfo = m_conversionDataPtr;
        RED::ITransformShape* itransform = sceneInfo->modelTransformShape->As<RED::ITransformShape>();

        // Apply transform matrix.
        RED::Matrix redMatrix;
        redMatrix.SetColumnMajorMatrix(matrix);

        // Restore left handedness transform.
        if (m_conversionDataPtr->viewHandedness == Handedness::LeftHanded) {
            //redMatrix = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * redMatrix;
        }

        RC_CHECK(itransform->SetMatrix(&redMatrix, iresourceManager->GetState()));
        resetFrame();

        return RED_RC();
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

    RED_RC removeSceneFromCamera(RED::Object* a_camera, LuminateSceneInfo const& a_sceneInfo)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Remove the scene from the camera.
        //////////////////////////////////////////

        RED::IViewpoint* iviewpoint = a_camera->As<RED::IViewpoint>();
        RC_TEST(iviewpoint->RemoveShape(a_sceneInfo.rootTransformShape, iresourceManager->GetState()));

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

        for (auto const& materialEntry: a_sceneInfo.materials) {
            RC_TEST(iresourceManager->DeleteMaterial(materialEntry.second, iresourceManager->GetState()));
        }

        for (auto const& imageEntry: a_sceneInfo.imageNameToLuminateMap) {
            RC_TEST(iresourceManager->DeleteImage(imageEntry.second, iresourceManager->GetState()));
        }

        return RED_OK;
    }

    RED_RC saveCameraAndSceneAsRedFile(RED::Object* a_sceneRootShape, RED::Object* a_camera, std::string const& a_filepath)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);

        //////////////////////////////////////////
        // Prepare the .red file.
        //////////////////////////////////////////

        RED::Object* red_file = RED::Factory::CreateInstance(CID_REDFile);
        RED::IREDFile* ired_file = red_file->As<RED::IREDFile>();

        RC_TEST(ired_file->Create(a_filepath.c_str(), true));

        //////////////////////////////////////////
        // Save the scene and the camera.
        //////////////////////////////////////////

        RED::StreamingPolicy policy;
        RC_TEST(ired_file->WriteDAG(a_sceneRootShape, policy, resourceManager));
        RC_TEST(ired_file->Write(a_camera, policy, resourceManager));

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

        if (a_sceneHandedness == Handedness::LeftHanded) {
            eyePosition = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * a_cameraInfo.eyePosition;
            sight = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * a_cameraInfo.sight;
            top = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * a_cameraInfo.top;
            right = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * a_cameraInfo.right;
            target = HoopsLuminateBridge::s_leftHandedToRightHandedMatrix * a_cameraInfo.target;
        }
        else if (a_sceneHandedness == Handedness::RightHanded) {
            // Even with 3DF/HPS right-handed we still need to flip the x axis.
            // For some reasons, 3DF seems to add a - on x axis when computing the view matrix.
            // To get similar result in RED we must mimic it.
            right = -right;
        }

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

} // namespace hoops_luminate_bridge
