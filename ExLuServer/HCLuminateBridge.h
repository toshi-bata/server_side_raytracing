#ifndef LUMINATEBRIDGE_COMMUNICATORLUMINATEBRIDGE_H
#define LUMINATEBRIDGE_COMMUNICATORLUMINATEBRIDGE_H

#include <string>
#include <memory>

#include <RED.h>
#include <REDObject.h>
#include <REDVector3.h>
#include <REDIWindow.h>

#include "ConversionTools.h"
#include "AxisTriad.h"
#include "LightingEnvironment.h"
#include "ExProcess.h"

namespace HC_luminate_bridge {

	/**
	* Enumeration of different lighting model supported by the bridge.
	*/
	enum class LightingModel { No, Default, PhysicalSunSky, EnvironmentMap };

	/**
	 * Structure storing generic information about a camera.
	 */
	struct CameraInfo {
		ProjectionMode projectionMode;
		float field_width;
		float field_height;
		RED::Vector3 eyePosition;
		RED::Vector3 right;
		RED::Vector3 top;
		RED::Vector3 sight;
		RED::Vector3 target;
	};

	/**
	 * Structure storing the frame statistics.
	 */
	struct FrameStatistics {
		bool renderingIsDone;
		float renderingProgress;
		float remainingTimeMilliseconds;
		int numberOfPasses;
		int currentPass;
	};

	/**
	 * Structure storing the information about selected segment.
	 */
	struct SelectedSegmentInfo {
		RED::Color m_diffuseColor;

		SelectedSegmentInfo(RED::Color const& a_diffuseColor): m_diffuseColor(a_diffuseColor) {};
		virtual ~SelectedSegmentInfo() = 0;
	};

	using SelectedSegmentInfoPtr = std::shared_ptr<SelectedSegmentInfo>;

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

	/**
	 * Base virtual class of helpers making usage of the bridge really easy.
	 */
	class HCLuminateBridge {
		// Instance Fields:
		// ----------------

	private:
		// Window.
		RED::Object* m_window;
		
		// Camera.
		bool m_bSyncCamera;
		CameraInfo m_cameraInfo;

		// Scene.
		LuminateSceneInfoPtr m_conversionDataPtr;

		// Render.
		bool m_frameIsComplete;
		bool m_newFrameIsRequired;
		FrameStatistics m_lastFrameStatistics;
		RED::FRAME_TRACING_FEEDBACK m_frameTracingMode;

		// Axis triad.
		AxisTriad m_axisTriad;

		// Lighting environment.
		// Only one of the two can be active at same time.
		LightingModel m_lightingModel;
		EnvironmentMapLightingModel m_environmentMapLightingModel;

		// segment update
		bool m_selectedSegmentTransformIsDirty;

		// Static Fields:
		// --------------
	public:
		static const RED::Matrix s_leftHandedToRightHandedMatrix;

		// Constructors:
		// -------------
	public:
		HCLuminateBridge();
		~HCLuminateBridge();

		// Methods:
		// --------

	public:
		/**
		* Request to draw a new frame.
		* This method must be called continuously and will check by itself
		* if cameras must be sync and if a new render must be processed.
		* @return True if success, otherwise False.
		*/
		bool draw();

		/**
		 * Requests to start a fresh new frame.
		 */
		void resetFrame();

		/**
		 * Synchronize Luminate scene with the current scene.
		 * The previous scene will be destroyed.
		 * @return True if success, otherwise False.
		 */
		bool syncScene(const int a_windowWidth, const int a_windowHeight, std::vector<MeshPropaties>, CameraInfo a_cameraInfo);

		/**
		 * Shutdown completely Luminate.
		 * This will destroy and free everything.
		 * @return True if success, otherwise False.
		 */
		bool shutdown();

		/**
		 * Resize the Luminate window.
		 * @param[in] a_windowWidth New width to set.
		 * @param[in] a_windowHeight New height to set.
		 * @return True if success, otherwise False.
		 */
		bool resize(int a_windowWidth, int a_windowHeight, CameraInfo a_cameraInfo);

		bool initialize(void* a_osHandle, int a_windowWidth, int a_windowHeight,
			std::string const& a_environmentMapFilepath, CameraInfo a_cameraInfo);

		/**
		 * Get rendering progress.
		 * @return rendering progress in percent.
		 */
		FrameStatistics getFrameStatistics();

		/**
		 * Get selected luminate transform node.
		 * @return Selected luminate transform node object pointer.
		 */
		RED::Object* getSelectedLuminateTransformNode(char* a_node_name);
		
		RED::Object* getFloorMesh();

		/**
		 * Get the Luminate window instance.
		 * @return Luminate window instance.
		 */
		RED::Object* getWindow() { return m_window; }

		RED::Color getSelectedLuminateDeffuseColor(char* a_node_name);

		/**
		 * Sets current lighting environment as a default model.
		 */
		RED_RC setDefaultLightEnvironment();

		/**
		 * Sets current lighting environment as a physical sun/sky model.
		 */
		RED_RC setSunSkyLightEnvironment();

		/**
		 * Sets current lighting environment as environment map.
		 * @param[in] a_imageFilepath Filepath of the env map file to use.
		 * @param[in] a_showImage Whether to show the image, if not it will only act as sky light.
		 * @param[in] a_backgroundColor Background color to show if the image is not shown.
		 */
		RED_RC
			setEnvMapLightEnvironment(std::string const& a_imageFilepath, bool a_showImage, RED::Color const& a_backgroundColor, const char* thumbFilePath = NULL);

		/**
		* Set sync camera.
		* @param[in] a_sync Whether to synchronize cametra between HC and Luminate.
		 */
		void setSyncCamera(const bool a_sync, CameraInfo a_cameraInfo) { m_bSyncCamera = a_sync; m_cameraInfo = a_cameraInfo; }

		CameraInfo creteCameraInfo(double* a_target, double* a_up, double* a_position, int a_projection, double a_width, double a_height);
		
		bool saveImg(const char* filePath);

		RED_RC syncModelTransform(double* matrix);

		bool addFloorMesh(const int pointCnt, const double* points, const int faceCnt, const int* faceList);
		bool deleteFloorMesh();

	private:
		/**
		 * Removes the current lighting environment from the scene.
		 */
		RED_RC removeCurrentLightingEnvironment();

		RED_RC createCamera(RED::Object* a_window, int a_windowWidh, int a_windowHeight, int a_vrlId, RED::Object*& a_outCamera);

		/**
		 * Synchronize the Luminate camera with the HC one.
		 * @return RED_OK if success, otherwise error code.
		 */
		RED_RC syncLuminateCamera(CameraInfo a_cameraInfo);

		LuminateSceneInfoPtr convertScene(std::vector <MeshPropaties> aMeshProps);
	};

	RED_RC setLicense(char const* a_license, bool& a_outLicenseIsActive);
	RED_RC setSoftTracerMode(int a_mode);
	RED_RC createRedWindow(void* a_osHandler,
		int a_width,
		int a_height,
#ifdef _LIN32
		Display* a_display,
		int a_screen,
		Visual* a_visual,
#endif
		RED::Object*& a_outWindow);

	/**
	 * Stops window tracing and shutdown completely Luminate.
	 * This will destroy and free everything.
	 * @return True if success, otherwise False.
	 */
	RED_RC shutdownLuminate(RED::Object* a_window);

	/**
	 * Create a new camera attached to a window.
	 * @param[in] a_window Window on which to attach the new camera.
	 * @param[in] a_windowWidth Current window width.
	 * @param[in] a_windowHeight Current window height.
	 * @param[out] a_outCamera Output camera.
	 * @return RED_OK if success, otherwise error code.
	 */
	RED_RC createRedCamera(RED::Object* a_window, int a_windowWidth, int a_windowHeight, int a_vrlId, RED::Object*& a_outCamera);

	/**
	 * Resize the Luminate window.
	 * @param[in] a_windowWidth New width to set.
	 * @param[in] a_windowHeight New height to set.
	 * @return RED_OK if success, otherwise error code.
	 */
	RED_RC resizeWindow(RED::Object* a_window, int a_newWidth, int a_newHeight);

	/**
	 * Add a scene to a camera.
	 * @param[in] a_camera Camera which will receive the scene root.
	 * @param[in] a_sceneInfo Scene description to add.
	 * @return RED_OK if success, otherwise error code.
	 */
	RED_RC addSceneToCamera(RED::Object* a_camera, LuminateSceneInfo const& a_sceneInfo);

	/**
	 * Destroy a scene and free its resources.
	 * @param[in] a_sceneInfo Description of scene to remove.
	 * @return RED_OK if success, otherwise error code.
	 */
	RED_RC destroyScene(LuminateSceneInfo const& a_sceneInfo);

	/**
 * Synchornize Luminate camera with a generic camera information.
	 * @param[in] a_camera Camera to synchronize.
	 * @param[in] a_sceneHandedness Scene handedness.
	 * @param[in] a_windowWidth Parent window width.
	 * @param[in] a_windowHeight Parent window height.
	 * @param[in] a_cameraInfo Camera generic description.
	 * @return RED_OK if success, otherwise error code.
	 */
	RED_RC syncCameras(RED::Object* a_camera,
		Handedness a_sceneHandedness,
		int a_windowWidth,
		int a_windowHeight,
		CameraInfo const& a_cameraInfo);

	/**
	 * Check and perform tracing call in a window.
	 * @param[in] a_window Window where to trace.
	 * @param[in] a_tracingMode Tracing methode, could be pathtracing or raytracing.
	 * @param[io] a_ioFrameIsComplete Input/Output flag indicating if current image has been fully traced.
	 * @param[io] a_ioNewFrameIsRequired Input/Output flag indicating if a new image should be started.
	 * @return RED_OK if success, otherwise error code.
	 */
	RED_RC checkDrawTracing(RED::Object* a_window,
		RED::FRAME_TRACING_FEEDBACK a_tracingMode,
		bool& a_ioFrameIsComplete,
		bool& a_ioNewFrameIsRequired);

	RED_RC checkDrawHardware(RED::Object* a_window);

	RED_RC checkFrameStatistics(RED::Object* a_window, const int a_num_vrl, FrameStatistics* a_stats, bool& a_ioFrameIsComplete);

	RED::Object* convertNordTree(RED::Object* a_resourceManager, ConversionContextNode& a_ioConversionContext, MeshPropaties aMeshProps);

	RED::Object* convertHEMeshToREDMeshShape(const RED::State& a_state, A3DMeshData a_meshData);

} // HC_luminate_bridge

#endif