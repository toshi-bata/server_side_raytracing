#include "HCLuminateBridge.h"

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

HCLuminateBridge::HCLuminateBridge() :
    m_frameIsComplete(false), m_newFrameIsRequired(true), m_frameTracingMode(RED::FTF_PATH_TRACING)
{

}

HCLuminateBridge::~HCLuminateBridge() 
{
}

bool HCLuminateBridge::initialize(std::string const& a_license, void* a_osHandle, int a_windowWidth, int a_windowHeight)
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
    // Synchronize 3DF and Luminate cameras for
    // the first time.
    //////////////////////////////////////////

    rc = syncLuminateCamera();
    if (rc != RED_OK)
        return false;

	return true;
}

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

    return syncLuminateCamera() == RED_OK;
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

bool HCLuminateBridge::saveImg()
{
    RED_RC rc;

    RED::IViewpointRenderList* defaultVRL = m_auxvrl->As<RED::IViewpointRenderList>();
    RED::Object* renderimg = defaultVRL->GetRenderImage();

    rc = RED::ImageTools::Save(renderimg, false, "C:\\temp\\my_image.png", false, true, 1.0);

    return true;
}

RED_RC HCLuminateBridge::syncLuminateCamera()
{
    RED::Object* resmgr = RED::Factory::CreateInstance(CID_REDResourceManager);
    RED::IResourceManager* iresmgr = resmgr->As<RED::IResourceManager>();

    // 2.1.1) Creating an offscreen cube with light and material:
    // ----------------------------------------------------------

    RED::Object* matr;
    RC_TEST(iresmgr->CreateMaterial(matr, iresmgr->GetState()));
    RED::IMaterial* imatr = matr->As< RED::IMaterial >();
    (imatr->SetupGenericDiffuseMaterial(false, RED::Color::RED, NULL, RED::Matrix::IDENTITY, RED::MCL_TEX0,
        &RED::LayerSet::ALL_LAYERS, NULL, resmgr, iresmgr->GetState()));

    RED::Object* auxnode = RED::Factory::CreateInstance(CID_REDTransformShape);
    if (!auxnode)
        RC_TEST(RED_ALLOC_FAILURE);

    auxnode->SetID("auxnode");

    RED::ITransformShape* iauxnode = auxnode->As< RED::ITransformShape >();

    RED::Object* auxcube = RED::Factory::CreateInstance(CID_REDMeshShape);
    if (!auxcube)
        RC_TEST(RED_FAIL);
    RED::IShape* isauxcube = auxcube->As< RED::IShape >();
    RC_TEST(isauxcube->SetMaterial(matr, iresmgr->GetState()));
    RED::IMeshShape* imauxcube = auxcube->As< RED::IMeshShape >();
    RC_TEST(imauxcube->Box(RED::Vector3(0.0, 0.0, 0.0), RED::Vector3(100.0, 100.0, 100.0), iresmgr->GetState()));
    RC_TEST(iauxnode->AddChild(auxcube, RED_SHP_DAG_NO_UPDATE, iresmgr->GetState()));

    RED::Object* auxlight = RED::Factory::CreateInstance(CID_REDLightShape);
    if (!auxlight)
        RC_TEST(RED_FAIL);
    RED::ILightShape* iauxlight = auxlight->As< RED::ILightShape >();
    float attvalues[RED_LIGHT_ATT_NB_VALUES] = { 1.5, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
    RC_TEST(iauxlight->SetPointLight(RED::ATN_NONE, attvalues, RED::Vector3(500.0, 600.0, 800.0),
        RED::Color::WHITE, RED::Color::WHITE, iresmgr->GetState()));


    // 2.1.2) Creating the offscreen camera:
    // -------------------------------------

    //RED::Object* auxcamera = RED::Factory::CreateInstance(CID_REDViewpoint);
    //if (!auxcamera)
    //    RC_TEST(RED_ALLOC_FAILURE);

    RED::IViewpoint* iauxcamera = m_auxcamera->As< RED::IViewpoint >();

    RED::Vector3 eye(250.0, 250.0, 250.0);
    RED::Vector3 sight, top, right;

    sight = -eye;
    sight.Normalize();
    top = RED::Vector3::ZAXIS;
    right = sight.Cross2(top);
    top = right.Cross2(sight);

    RC_TEST(iauxcamera->SetEye(eye, iresmgr->GetState()));
    RC_TEST(iauxcamera->SetViewingAxis(sight, top, right, iresmgr->GetState()));
    RC_TEST(iauxcamera->SetFrustumPerspective(RED_PI / 6.0, 1.0, iresmgr->GetState()));
    RC_TEST(iauxcamera->SetNearFar(1.0, 1000.0, iresmgr->GetState()));

    RC_TEST(iauxcamera->AddShape(auxnode, iresmgr->GetState()));
    RC_TEST(iauxcamera->AddShape(auxlight, iresmgr->GetState()));

    return RED_OK;
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
        const RED::ViewpointStatistics& camstats = fstats.GetViewpointStatistics(0, 1);

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