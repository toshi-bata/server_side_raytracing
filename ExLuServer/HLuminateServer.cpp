#include "HLuminateServer.h"
#include <cassert>
#include <REDObject.h>
#include <REDFactory.h>
#include <REDIResourceManager.h>
#include <REDILicense.h>
#include <REDIOptions.h>
#include <REDIShape.h>
#include <REDIREDFile.h>
#include <REDIDataManager.h>
#include <REDIMaterialController.h>
#include <REDIMaterialControllerProperty.h>
#include "hoops_license.h"

#define RC_CHECK(rc)                                                               \
    {                                                                              \
        RED_RC red_rc_check = (rc);                                                \
        if (red_rc_check != RED_OK) {                                              \
            printf("RC_CHECK failed :: " #rc " :: code = 0x%04x\n", red_rc_check); \
            assert(false);                                                         \
        }                                                                          \
    }

using namespace hoops_luminate_bridge;
#ifndef _WIN32
#else
//******************************************************************************
//*** Win32 WindowProc *********************************************************
//******************************************************************************

LRESULT CALLBACK WindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_CREATE)
    {
        CREATESTRUCT* tpCreateSt = (CREATESTRUCT*)lParam;
        ::ShowWindow(hWnd, SW_SHOW);
    }
    else if (Msg == WM_CLOSE)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, Msg, wParam, lParam);
}

HWND CreateWndow(const int width, const int height)
{
    // Create a custom Win32 class:
    WNDCLASSW wc;
    HMODULE hInstance = GetModuleHandle(NULL);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
    wc.lpfnWndProc = (WNDPROC)WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = L"WindowClass";

    RegisterClassW(&wc);
    HWND hwnd = CreateWindowEx(WS_EX_CLIENTEDGE, L"WindowClass", L"myLuminateWindow",
        WS_MINIMIZE | WS_EX_TOOLWINDOW,
        100, 100, width, height, NULL, NULL, hInstance, NULL);

    return hwnd;
}
#endif

bool HLuminateServer::Terminate()
{
    //////////////////////////////////////////
    // Get the resource manager singleton.
    //////////////////////////////////////////

    RED::Object* reamgr = RED::Factory::CreateInstance(CID_REDResourceManager);
    RED::IResourceManager* ireamgr = reamgr->As<RED::IResourceManager>();

    //////////////////////////////////////////
     // Stop all tracing threads.
     //////////////////////////////////////////

    for (auto it = m_mHLuminateSession.begin(); it != m_mHLuminateSession.end(); ++it)
    {
        LuminateSession lumSession = it->second;
        stopFrameTracing(lumSession.pHCLuminateBridge);
    }
    std::map<std::string, LuminateSession>().swap(m_mHLuminateSession);

    //////////////////////////////////////////
    // Destroy the resource manager.
    //////////////////////////////////////////

    RED::Factory::DeleteInstance(reamgr, ireamgr->GetState());

    return true;
}

bool HLuminateServer::PrepareRendering(std::string sessionId, 
    double* target, double* up, double* position, int projection, double cameraW, double cameraH, 
    int width, int height)
{
    LuminateSession lumSession;

    if (0 == m_mHLuminateSession.count(sessionId))
    {
        lumSession.pHCLuminateBridge = new HoopsLuminateBridgeEx();

        CameraInfo cameraInfo = lumSession.pHCLuminateBridge->creteCameraInfo(target, up, position, projection, cameraW, cameraH);

        //lumSession.hwnd = CreateWndow(0, 0);
        lumSession.hwnd = GetConsoleWindow();

        std::string filepath = "";
        lumSession.pHCLuminateBridge->initialize(HOOPS_LICENSE, lumSession.hwnd, width, height, filepath, cameraInfo);

        m_mHLuminateSession[sessionId] = lumSession;
    }
    else
    {
        lumSession = m_mHLuminateSession[sessionId];
    }
    lumSession.pHCLuminateBridge->draw();
    
    return true;
}

bool HLuminateServer::StartRendering(std::string sessionId,
    double* target, double* up, double* position, int projection, double cameraW, double cameraH,
    int width, int height, A3DAsmModelFile* pModelFile)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        CameraInfo cameraInfo = lumSession.pHCLuminateBridge->creteCameraInfo(target, up, position, projection, cameraW, cameraH);

        lumSession.pHCLuminateBridge->setModelFile(pModelFile);
        
        lumSession.pHCLuminateBridge->syncScene(width, height, cameraInfo);

        return true;
    }
    return false;
}

std::vector<float> HLuminateServer::Draw(std::string sessionId, char* filePath)
{
    std::vector<float> floatArr;
    
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        for (int i = 0; i < 10; i++)
            lumSession.pHCLuminateBridge->draw();

        FrameStatistics statistics = lumSession.pHCLuminateBridge->getFrameStatistics();

        floatArr.push_back(statistics.renderingIsDone);
        floatArr.push_back(statistics.renderingProgress);
        floatArr.push_back(statistics.remainingTimeMilliseconds);

        lumSession.pHCLuminateBridge->saveImg(filePath);
    }
    return floatArr;
}

bool HLuminateServer::ClearSession(std::string sessionId)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        RED_RC rc;
        RED::Object* reamgr = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* ireamgr = reamgr->As<RED::IResourceManager>();

        LuminateSession lumSession = m_mHLuminateSession[sessionId];
        lumSession.pHCLuminateBridge->resetFrame();

        // Delete env map
        for (int i = 0; i < lumSession.envMapArr.size(); i++)
        {
            if (lumSession.envMapArr[i].imagePath != "") {
                ireamgr->DeleteImage(lumSession.envMapArr[i].backgroundCubeImage, ireamgr->GetState());
                rc = RED::Factory::DeleteInstance(lumSession.envMapArr[i].skyLight, ireamgr->GetState());
            }
        }

        lumSession.pHCLuminateBridge->shutdown();

        delete lumSession.pHCLuminateBridge;

        //if (NULL != lumSession.hwnd)
        //    DestroyWindow(lumSession.hwnd);

        m_mHLuminateSession.erase(sessionId);

        return true;
    }
    return false;
}

bool HLuminateServer::LoadEnvMapFile(std::string sessionId, const char* filePath, const char* thumbnailPath)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];
        lumSession.pHCLuminateBridge->resetFrame();

        EnvironmentMapLightingModel envMap;
        if (RED_OK != lumSession.pHCLuminateBridge->createEnvMapLightEnvironment(filePath, true, RED::Color::WHITE, thumbnailPath, envMap))
            return false;

        m_mHLuminateSession[sessionId].envMapArr.push_back(envMap);

        return true;
    }
    return false;
}

bool HLuminateServer::SyncCamera(std::string sessionId,
    double* target, double* up, double* position, int projection, double cameraW, double cameraH)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        CameraInfo cameraInfo = lumSession.pHCLuminateBridge->creteCameraInfo(target, up, position, projection, cameraW, cameraH);

        lumSession.pHCLuminateBridge->setSyncCamera(true, cameraInfo);

        return true;
    }
    return false;
}

bool HLuminateServer::Resize(std::string sessionId,
    double* target, double* up, double* position, int projection, double cameraW, double cameraH,
    int width, int height)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        CameraInfo cameraInfo = lumSession.pHCLuminateBridge->creteCameraInfo(target, up, position, projection, cameraW, cameraH);

        lumSession.pHCLuminateBridge->resize(width, height, cameraInfo);

        return true;
    }
    return false;
}

void HLuminateServer::stopFrameTracing(HoopsLuminateBridge* bridge)
{
    RED::Object* resmgr = RED::Factory::CreateInstance(CID_REDResourceManager);
    RED::IResourceManager* iresmgr = resmgr->As<RED::IResourceManager>();

    if (iresmgr->GetState().GetNumber() > 1) {
        RED::IWindow* iwindow = bridge->getWindow()->As<RED::IWindow>();
        iwindow->FrameTracingStop();

        iresmgr->BeginState();
    }

    bridge->resetFrame();
}

bool HLuminateServer::loadLibMaterial(HoopsLuminateBridge* bridge, RED::String redfilename, RED::Object*& libraryMaterial)
{
    RED::Object* resmgr = RED::Factory::CreateInstance(CID_REDResourceManager);
    RED::IResourceManager* iresmgr = resmgr->As<RED::IResourceManager>();

    // As a new material will be created, some images will be created as well.
    // Or images operations are immediate and should occur without ongoing rendering.
    // Thus we need to stop current frame tracing.
    stopFrameTracing(bridge);

    // create the file instance
    RED::Object* file = RED::Factory::CreateInstance(CID_REDFile);
    RED::IREDFile* ifile = file->As<RED::IREDFile>();

    // load the file
    RED::StreamingPolicy policy;
    RED::FileHeader fheader;
    RED::FileInfo finfo;
    RED::Vector< unsigned int > contexts;

    RC_CHECK(ifile->Load(redfilename, iresmgr->GetState(), policy, fheader, finfo, contexts));

    // release the file
    RC_CHECK(RED::Factory::DeleteInstance(file, iresmgr->GetState()));

    // retrieve the data manager
    RED::IDataManager* idatamgr = iresmgr->GetDataManager()->As<RED::IDataManager>();

    // parse the loaded contexts looking for the first material.
    for (unsigned int c = 0; c < contexts.size(); ++c) {
        unsigned int mcount;
        RC_CHECK(idatamgr->GetMaterialsCount(mcount, contexts[c]));

        if (mcount > 0) {
            RC_CHECK(idatamgr->GetMaterial(libraryMaterial, contexts[c], 0));
            break;
        }
    }

    return true;
}

bool HLuminateServer::SetMaterial(std::string sessionId, const char* nodeName, RED::String redfilename, bool overrideMaterial, bool preserveColor)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        // Apply material
        HoopsLuminateBridgeEx* bridge = lumSession.pHCLuminateBridge;
        RED::Object* selectedTransformNode = bridge->getSelectedLuminateTransformNode((char*)nodeName);

        if (selectedTransformNode != nullptr) {
            RED::Object* resmgr = RED::Factory::CreateInstance(CID_REDResourceManager);
            RED::IResourceManager* iresmgr = resmgr->As<RED::IResourceManager>();
            RED::IShape* iShape = selectedTransformNode->As<RED::IShape>();

            RED::Object* libraryMaterial = nullptr;
            loadLibMaterial(bridge, redfilename, libraryMaterial);

            if (libraryMaterial != nullptr) {
                // Clone the material to be able to change its properties without altering the library one.
                RED::Object* clonedMaterial;
                RC_CHECK(iresmgr->CloneMaterial(clonedMaterial, libraryMaterial, iresmgr->GetState()));

                if (preserveColor)
                {
                    // Duplicate the material controller
                    RED_RC returnCode;
                    RED::Object* materialController = iresmgr->GetMaterialController(libraryMaterial);
                    RED::Object* clonedMaterialController = RED::Factory::CreateMaterialController(*resmgr,
                        clonedMaterial,
                        "Realistic",
                        "",
                        "Tunable realistic material",
                        "Realistic",
                        "Redway3d",
                        returnCode);

                    RC_CHECK(returnCode);
                    RED::IMaterialController* clonedIMaterialController =
                        clonedMaterialController->As<RED::IMaterialController>();
                    RC_CHECK(clonedIMaterialController->CopyFrom(*materialController, clonedMaterial));

                    // Set node diffuse color as Luminate diffuse + reflection.

                    RED::Object* diffuseColorProperty = clonedIMaterialController->GetProperty(RED_MATCTRL_DIFFUSE_COLOR);
                    RED::IMaterialControllerProperty* iDiffuseColorProperty =
                        diffuseColorProperty->As<RED::IMaterialControllerProperty>();
                    iDiffuseColorProperty->SetColor(bridge->getSelectedLuminateDeffuseColor((char*)nodeName),
                        iresmgr->GetState());

                }

                RED::Object* currentMaterial;
                iShape->GetMaterial(currentMaterial);

                if (overrideMaterial)
                {
                    iShape->SetMaterial(clonedMaterial, iresmgr->GetState());
                }
                else
                {
                    RED::IMaterial* currentIMaterial = currentMaterial->As<RED::IMaterial>();
                    RC_CHECK(currentIMaterial->CopyFrom(*clonedMaterial, iresmgr->GetState()));
                }

                bridge->resetFrame();
            }
        }

        //lumSession.pHCLuminateBridge->applyMaterial(nodeName, redfilename, overrideMaterial, preserveColor);

        return true;
    }
    return false;
}
bool HLuminateServer::SetLighting(std::string sessionId, int lightingId)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        switch (lightingId)
        {
        case 0: lumSession.pHCLuminateBridge->setDefaultLightEnvironment(); break;
        case 1: lumSession.pHCLuminateBridge->setSunSkyLightEnvironment(); break;
        default:
            int envMapId = lightingId - 2;
            lumSession.pHCLuminateBridge->setEnvMapLightEnvironment(lumSession.envMapArr[envMapId]);
            break;
        }

        return true;
    }
    return false;
}

bool HLuminateServer::SetModelTransform(std::string sessionId, double* matrix)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        lumSession.pHCLuminateBridge->syncModelTransform(matrix);

        return true;
    }
    return false;
}

bool HLuminateServer::DownloadImage(std::string sessionId)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];
        return true;
    }
    return false;
}

bool HLuminateServer::AddFloorMesh(const std::string sessionId, const int pointCnt, const double* points, const int faceCnt, const int* faceList, const double* uvs)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        if (lumSession.pHCLuminateBridge->addFloorMesh(pointCnt, points, faceCnt, faceList, uvs))
        {
            RED::Object* mesh = lumSession.pHCLuminateBridge->getFloorMesh();
            if (nullptr == mesh)
                return false;

            return true;
        }

        return false;
    }
    return false;
}

bool HLuminateServer::DeleteFloorMesh(const std::string sessionId)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        lumSession.pHCLuminateBridge->deleteFloorMesh();

        return true;
    }
    return false;
}

bool HLuminateServer::UpdateFloorMaterial(const std::string sessionId, const double* color, const char* texturePath, const double uvScale)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        lumSession.pHCLuminateBridge->updateFloorMaterial(color, texturePath, uvScale);

        return true;
    }
    return false;
}

int HLuminateServer::GetNewEnvMapId(const std::string sessionId)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        return lumSession.envMapArr.size();
    }
    return -1;
}