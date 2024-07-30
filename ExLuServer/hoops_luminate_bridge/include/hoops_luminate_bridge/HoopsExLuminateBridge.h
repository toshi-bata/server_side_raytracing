#ifndef LUMINATEBRIDGE_HOOPSCOMMUNICATORLUMINATEBRIDGE_H
#define LUMINATEBRIDGE_HOOPSCOMMUNICATORLUMINATEBRIDGE_H

#include "HoopsLuminateBridge.h"
#include "ConversionTools.h"
#include <A3DSDKIncludes.h>

namespace hoops_luminate_bridge {
	/**
	 * Mapping between segment hash and associated Luminate mesh shapes.
	 */
	using SegmentMeshShapesMap = std::map<intptr_t, std::vector<RED::Object*>>;

	/**
	 * Mapping between segment hash and associated Luminate transform shape.
	 */
	using SegmentTransformShapeMap = std::map<std::string, RED::Object*>;

	/**
	* LuminateSceneInfo extension with specific node informations.
	*/
	struct ConversionContextNode : LuminateSceneInfo {
		SegmentMeshShapesMap segmentMeshShapesMap;
		SegmentTransformShapeMap segmentTransformShapeMap;
		std::map<std::string, RED::Color> nodeDiffuseColorMap;
	};

	using ConversionContextNodePtr = std::shared_ptr<ConversionContextNode>;

	class HoopsLuminateBridgeEx : public HoopsLuminateBridge
	{
	public:
		HoopsLuminateBridgeEx();
		~HoopsLuminateBridgeEx();

	private:
		A3DAsmModelFile* m_pModelFile;

		// Floor UV param
		std::vector<float> m_floorUVArr;

	public:
		void saveCameraState() override;
		LuminateSceneInfoPtr convertScene() override;
		bool checkCameraChange() override;
		CameraInfo getCameraInfo() override;
		void updateSelectedSegmentTransform() override;
		std::vector<RED::Object*> getSelectedLuminateMeshShapes() override;
		RED::Object* getSelectedLuminateTransformNode(char* a_node_name) override;
		RED::Color getSelectedLuminateDeffuseColor(char* a_node_name) override;
		RED_RC syncRootTransform() override;

	public:
		void setModelFile(A3DAsmModelFile* pModelFile) { m_pModelFile = pModelFile; }
		bool addFloorMesh(const int pointCnt, const double* points, const int faceCnt, const int* faceList, const double* uvs);
		bool deleteFloorMesh();
		bool updateFloorMaterial(const double* color, const char* texturePath, const double uvScale = 0.0);
		RED::Object* getFloorMesh();

	};

	LuminateSceneInfoPtr convertExSceneToLuminate(A3DAsmModelFile* pModelFile);
	RealisticMaterialInfo getSegmentMaterialInfo(A3DGraphRgbColorData a_sColor,
		RED::Object* a_resourceManager,
		RealisticMaterialInfo const& a_baseMaterialInfo,
		ImageNameToLuminateMap& a_ioImageNameToLuminateMap,
		TextureNameImageNameMap& a_ioTextureNameImageNameMap,
		PBRToRealisticConversionMap& a_ioPBRToRealisticConversionMap);
}
#endif
