#include "HLuminateServer.h"
#include <REDObject.h>
#include <REDFactory.h>
#include <REDIResourceManager.h>
#include <REDILicense.h>
#include <REDIOptions.h>

using namespace HC_luminate_bridge;

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
        WS_SYSMENU | WS_BORDER | WS_CAPTION | WS_VISIBLE,
        100, 100, width, height, NULL, NULL, hInstance, NULL);

    return hwnd;
}

bool HLuminateServer::Init(char const* license)
{
    RED_RC rc;

    //////////////////////////////////////////
    // Get the resource manager singleton
    //////////////////////////////////////////

    RED::Object* resmgr = RED::Factory::CreateInstance(CID_REDResourceManager);
    if (!resmgr)
        return false;

    RED::IResourceManager* iresmgr = resmgr->As< RED::IResourceManager >();

    //////////////////////////////////////////
    // Set the license and check its validity
    //////////////////////////////////////////

    RED::ILicense* ilicense = resmgr->As<RED::ILicense>();
    if (RED_OK != ilicense->SetLicense(license))
        return false;

    bool isActivate = false;
    rc = ilicense->IsProductActivated(isActivate, RED::PROD_REDSDK);
    
    if (!isActivate)
        return false;

    //////////////////////////////////////////
    // Set tracer mode
    //////////////////////////////////////////

    RED::IOptions* ioptions = resmgr->As<RED::IOptions>();
    if (RED_OK != ioptions->SetOptionValue(RED::OPTIONS_RAY_ENABLE_SOFT_TRACER, 1, iresmgr->GetState()))
        return false;

    return true;
}

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
        lumSession.pHCLuminateBridge->stopFrameTracing();
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
        lumSession.pHCLuminateBridge = new HCLuminateBridge();

        CameraInfo cameraInfo = lumSession.pHCLuminateBridge->creteCameraInfo(target, up, position, projection, cameraW, cameraH);

        lumSession.hwnd = CreateWndow(0, 0);
        std::string filepath = "";
        lumSession.pHCLuminateBridge->initialize(lumSession.hwnd, width, height, filepath, cameraInfo);

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
    int width, int height, std::vector<MeshPropaties> aMeshProps)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        CameraInfo cameraInfo = lumSession.pHCLuminateBridge->creteCameraInfo(target, up, position, projection, cameraW, cameraH);

        lumSession.pHCLuminateBridge->syncScene(width, height, aMeshProps, cameraInfo);

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
        LuminateSession lumSession = m_mHLuminateSession[sessionId];
        delete lumSession.pHCLuminateBridge;

        if (NULL != lumSession.hwnd)
            DestroyWindow(lumSession.hwnd);

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

        if (RED_OK != lumSession.pHCLuminateBridge->setEnvMapLightEnvironment(filePath, true, RED::Color::WHITE, thumbnailPath))
            return false;

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

bool HLuminateServer::SetMaterial(std::string sessionId, const char* nodeName, RED::String redfilename, bool overrideMaterial, bool preserveColor)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        lumSession.pHCLuminateBridge->applyMaterial(nodeName, redfilename, overrideMaterial, preserveColor);

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
        case 3: lumSession.pHCLuminateBridge->setEnvMapLightEnvironment("", true, RED::Color::WHITE); break;
            break;
        default: break;
        }

        return true;
    }
    return false;
}

bool HLuminateServer::SetRootTransform(std::string sessionId, double* matrix)
{
    if (m_mHLuminateSession.count(sessionId))
    {
        LuminateSession lumSession = m_mHLuminateSession[sessionId];

        lumSession.pHCLuminateBridge->syncRootTransform(matrix);

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