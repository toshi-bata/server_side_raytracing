#pragma once
#include <string>
#include <map>
#include <windows.h>
#include "HCLuminateBridge.h"
#include "ExProcess.h"

using namespace HC_luminate_bridge;

class HLuminateServer
{
public:
	HLuminateServer() {}
	~HLuminateServer(){}

private:
	struct LuminateSession
	{
		HCLuminateBridge* pHCLuminateBridge = NULL;
		HWND hwnd;
	};

	std::map<std::string, LuminateSession> m_mHLuminateSession;

	void stopFrameTracing(HCLuminateBridge* bridge);

public:
	bool Init(char const* license);
	bool Terminate();
	bool PrepareRendering(std::string sessionId, 
		double* target, double* up, double* position, int projection, double cameraW, double cameraH, 
		int width, int height);
	bool StartRendering(std::string sessionId,
		double* target, double* up, double* position, int projection, double cameraW, double cameraH,
		int width, int height, std::vector<MeshPropaties> aMeshProps);
	std::vector<float> Draw(std::string sessionId, char* filePath);
	bool ClearSession(std::string sessionId);
	bool LoadEnvMapFile(std::string a_sessionId, const char* filePath, const char* thumbnailPath);
	bool SyncCamera(std::string sessionId,
		double* target, double* up, double* position, int projection, double cameraW, double cameraH);
	bool Resize(std::string sessionId,
		double* target, double* up, double* position, int projection, double cameraW, double cameraH,
		int width, int height);
	bool SetMaterial(std::string sessionId, const char* nodeName, RED::String redfilename, bool overrideMaterial, bool preserveColor);
	bool SetLighting(std::string sessionId, int lightingId);
	bool SetRootTransform(std::string sessionId, double* matrix);
	bool DownloadImage(std::string sessionId);
};

