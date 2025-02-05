#pragma once
#include <string>
#include <map>
#include "hoops_luminate_bridge/include/hoops_luminate_bridge/HoopsExLuminateBridge.h"
#include "ExProcess.h"

using namespace hoops_luminate_bridge;

class HLuminateServer
{
public:
	HLuminateServer() {}
	~HLuminateServer(){}

private:
	struct LuminateSession
	{
		HoopsLuminateBridgeEx* pHCLuminateBridge = NULL;
		HWND hwnd;
		std::vector<EnvironmentMapLightingModel> envMapArr;
	};

	std::map<std::string, LuminateSession> m_mHLuminateSession;

	void stopFrameTracing(HoopsLuminateBridge* bridge);
	bool loadLibMaterial(HoopsLuminateBridge* bridge, RED::String redfilename, RED::Object*& libraryMaterial);

public:
	bool Terminate();
	bool PrepareRendering(std::string sessionId, 
		double* target, double* up, double* position, int projection, double cameraW, double cameraH, 
		int width, int height);
	bool StartRendering(std::string sessionId,
		double* target, double* up, double* position, int projection, double cameraW, double cameraH,
		int width, int height, A3DAsmModelFile* pModelFile, A3DEntity* pPrcIdMap);
	std::vector<float> Draw(std::string sessionId, char* filePath);
	bool ClearSession(std::string sessionId);
	bool LoadEnvMapFile(std::string sessionId, const char* filePath, const char* thumbnailPath);
	bool SyncCamera(std::string sessionId,
		double* target, double* up, double* position, int projection, double cameraW, double cameraH);
	bool Resize(std::string sessionId,
		double* target, double* up, double* position, int projection, double cameraW, double cameraH,
		int width, int height);
	bool SetMaterial(std::string sessionId, const char* nodeName, RED::String redfilename, bool overrideMaterial, bool preserveColor);
	bool SetLighting(std::string sessionId, int lightingId);
	bool SetModelTransform(std::string sessionId, double* matrix);
	bool DownloadImage(std::string sessionId);
	bool AddFloorMesh(const std::string sessionId, const int pointCnt, const double* points, const int faceCnt, const int* faceList, const double* uvs);
	bool DeleteFloorMesh(const std::string sessionId);
	bool UpdateFloorMaterial(const std::string sessionId, const double* color, const char* texturePath, const double uvScale = 0.0);
	int GetNewEnvMapId(const std::string sessionId);
};

