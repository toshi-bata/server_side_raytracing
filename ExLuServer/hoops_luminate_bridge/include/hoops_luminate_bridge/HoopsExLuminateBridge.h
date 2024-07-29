#ifndef LUMINATEBRIDGE_HOOPSCOMMUNICATORLUMINATEBRIDGE_H
#define LUMINATEBRIDGE_HOOPSCOMMUNICATORLUMINATEBRIDGE_H

#include "HoopsLuminateBridge.h"
#include "ConversionTools.h"
#include "..\..\ExProcess.h"

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
		std::vector<MeshPropaties> m_aMeshProps;

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
		void setMeshProps(std::vector<MeshPropaties> aMeshProps) { m_aMeshProps = aMeshProps; }
		bool addFloorMesh(const int pointCnt, const double* points, const int faceCnt, const int* faceList, const double* uvs);
		bool deleteFloorMesh();
		bool updateFloorMaterial(const double* color, const char* texturePath, const double uvScale = 0.0);
		RED::Object* getFloorMesh();

	};

	LuminateSceneInfoPtr convertExSceneToLuminate(std::vector<MeshPropaties> aMeshProps);
	RED::Object* convertNordTree(RED::Object* a_resourceManager, ConversionContextNode& a_ioConversionContext, MeshPropaties meshProps);
	RealisticMaterialInfo getSegmentMaterialInfo(MeshPropaties meshProps,
		RED::Object* a_resourceManager,
		RealisticMaterialInfo const& a_baseMaterialInfo,
		ImageNameToLuminateMap& a_ioImageNameToLuminateMap,
		TextureNameImageNameMap& a_ioTextureNameImageNameMap,
		PBRToRealisticConversionMap& a_ioPBRToRealisticConversionMap);
	RED::Object* convertExMeshToREDMeshShape(const RED::State& a_state, A3DMeshData a_meshData);

}
#endif
