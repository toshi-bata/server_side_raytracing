#ifndef LUMINATEBRIDGE_HOOPSLUMINATEBRIDGE_H
#define LUMINATEBRIDGE_HOOPSLUMINATEBRIDGE_H

#include <string>
#include <memory>

#include <RED.h>
#include <REDObject.h>
#include <REDVector3.h>

#include <hoops_luminate_bridge/ConversionTools.h>
#include <hoops_luminate_bridge/AxisTriad.h>
#include <hoops_luminate_bridge/LightingEnvironment.h>

namespace hoops_luminate_bridge {

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

        SelectedSegmentInfo(RED::Color const& a_diffuseColor): m_diffuseColor(a_diffuseColor){};
        virtual ~SelectedSegmentInfo() = 0;
    };

    using SelectedSegmentInfoPtr = std::shared_ptr<SelectedSegmentInfo>;

    /**
     * Base virtual class of helpers making usage of the bridge really easy.
     * It is implemented by LuminateBridge3DF and LuminateBridgeHPS with 3DF/HPS
     * specificities.
     */
    class HoopsLuminateBridge {
        // Instance Fields:
        // ----------------

      protected:
        // Window.
        RED::Object* m_window;
        int m_windowWidth;
        int m_windowHeight;

        // Camera.
        RED::Object* m_camera;
        bool m_bSyncCamera;
        CameraInfo m_cameraInfo;

        // Scene.
        LuminateSceneInfoPtr m_conversionDataPtr;

        // Selection
        SelectedSegmentInfoPtr m_selectedSegment;

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
        DefaultLightingModel m_defaultLightingModel;
        PhysicalSunSkyLightingModel m_sunSkyLightingModel;
        EnvironmentMapLightingModel m_environmentMapLightingModel;

        // segment update
        bool m_selectedSegmentTransformIsDirty;
        bool m_rootTransformIsDirty;

        // Static Fields:
        // --------------
      public:
        static const RED::Matrix s_leftHandedToRightHandedMatrix;

        // Constructors:
        // -------------

      protected:
        HoopsLuminateBridge();

      public:
        virtual ~HoopsLuminateBridge();

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
         * Synchronize Luminate scene with the current 3DF/HPS scene.
         * The previous scene will be destroyed.
         * @return True if success, otherwise False.
         */
        bool syncScene(const int a_windowWidth, const int a_windowHeight, CameraInfo a_cameraInfo);

        /**
         * Save the current Luminate scene as a .red file.
         * Scene DAG and current camera state will be saved but
         * not the background image.
         * @param[in] a_outputFilepath Output filepath where to write the .red file.
         * @return True if success, otherwise False.
         */
        bool saveAsRedFile(std::string const& a_outputFilepath);

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

        /**
         * Initialise Luminate by setting the license and
         * creating a window.
         * @param[in] a_license Hoops unified license.
         * @param[in] a_osHandle OS handler. The HWND on Windows or the X-Window id on Linux/UNIX.
         * @param[in] a_windowWidth Initial window width.
         * @param[in] a_windowHeight Initial window height.
         * @param[in] a_display (Linux/UNIX only) pointer to the X display.
         * @param[in] a_screen (Linux/UNIX only) index of the screen.
         * @param[in] a_visual (Linux/UNIX only) pointer to the X visual.
         * @param[in] a_environmentMapFilepath Filepath to the image used to generate an environment map lighting environment.
         *                                     If empty, a sun/sky lighting environment will be used instead.
         * @return True if success, otherwise False.
         */
        bool initialize(std::string const& a_license,
                        void* a_osHandle,
                        int a_windowWidth,
                        int a_windowHeight,
#ifdef _LIN32
                        Display* a_display,
                        int a_screen,
                        Visual* a_visual,
#endif
                        std::string const& a_environmentMapFilepath, CameraInfo a_cameraInfo);

        /**
         * Flag the selected transform shape as dirty, requiring a sync
         * with its associated 3DF/HPS segment.
         */
        void invalidateSelectedSegmentTransform();

        /**
         * Flag the root transform shape as dirty, requiring a sync
         * with the associated 3DF/HPS root segment.
         */
        void invalidateRootTransform();

        /**
         * Get rendering progress.
         * @return rendering progress in percent.
         */
        FrameStatistics getFrameStatistics();

        /**
         * Get data generated during the scene conversion.
         * @return Conversion data.
         */
        LuminateSceneInfoPtr getConversionData() const;

        /**
         * Get selected segment information.
         * @return Selected segment information.
         */
        SelectedSegmentInfoPtr getSelectedSegmentInfo() const;

        /**
         * Get selected luminate meshShapes vector.
         * @return Vector of selected luminate meshShapes object pointers.
         */
        virtual std::vector<RED::Object*> getSelectedLuminateMeshShapes() = 0;

        /**
         * Get selected luminate transform node.
         * @return Selected luminate transform node object pointer.
         */
        virtual RED::Object* getSelectedLuminateTransformNode(char* a_node_name) = 0;

        virtual RED::Color getSelectedLuminateDeffuseColor(char* a_node_name) = 0;

        RED_RC syncModelTransform(double* matrix);

        /**
         * Set selected segment info.
         * @param[in] a_selectedSegmentInfo selected segment info struct.
         */
        void setSelectedSegmentInfo(SelectedSegmentInfoPtr a_selectedSegmentInfo);

        /**
         * Get the Luminate window instance.
         * @return Luminate window instance.
         */
        RED::Object* getWindow() const;

        /**
         * Set the Luminate frame tracing mode.
         * @param[in] a_mode Luminate frame tracing mode.
         */
        void setFrameTracingMode(int a_mode);

        /**
         * Set the Luminate software anti alliasing value.
         * @param[in] a_value Luminate software anti alliasing value.
         */
        void setSoftAAValue(int a_value);

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
        setEnvMapLightEnvironment(EnvironmentMapLightingModel envMap);

        /**
         * Create a new light.
         * @param[in] a_luminateLight Information about the current light to create.
         * @param[in] a_useCameraPosition If true, use camera position to create the light, otherwise use information from
         * luminate light.
         * @return RED_OK if success, otherwise error code.
         */
        RED_RC createLight(LuminateLight* a_luminateLight, bool a_useCameraPosition);

        /**
         * Create a new light.
         * @param[in] a_luminateLight Information about the current light to create.
         * @param[in] a_luminateBridge Luminate bridge
         * @return RED_OK if success, otherwise error code.
         */
        RED_RC removeLight(LuminateLight* a_luminateLight);

        void setSyncCamera(const bool a_sync, CameraInfo a_cameraInfo) { m_bSyncCamera = a_sync; m_cameraInfo = a_cameraInfo; }

        RED_RC createEnvMapLightEnvironment(std::string const& a_imageFilepath, bool a_showImage, RED::Color const& a_backgroundColor, const char* thumbFilePath, EnvironmentMapLightingModel& envMap);
        
        CameraInfo creteCameraInfo(double* a_target, double* a_up, double* a_position, int a_projection, double a_width, double a_height);

        bool saveImg(const char* filePath);

      private:
        /**
         * Check if the Luminate camera must be synchronized with 3DF/HPS one.
         * @param[in] a_force Whether to force the camera synchro even if camera has not changed, or not.
         * @return RED_OK if success, otherwise error code.
         */
        RED_RC checkCameraSync(CameraInfo a_cameraInfo);

        /**
         * Synchronize the Luminate camera with the 3DF/HPS one.
         * @return RED_OK if success, otherwise error code.
         */
        RED_RC syncLuminateCamera(CameraInfo a_cameraInfo);

        /**
         * Synchronize the Luminate root transform with the 3DF/HPS one.
         * @return RED_OK if success, otherwise error code.
         */
        virtual RED_RC syncRootTransform() = 0;

        // Pure Virtual Methods
        // --------------------

      protected:
        /**
         * Cache current 3DF/HPS camera state.
         */
        virtual void saveCameraState() = 0;

        /**
         * Check whether or not the 3DF/HPS camera has changed since last call.
         * @return True if the camera has changed, False otherwise.
         */
        virtual bool checkCameraChange() = 0;

        /**
         * Convert the 3DF/HPS scene to Luminate.
         * @return Luminate scene description.
         */
        virtual LuminateSceneInfoPtr convertScene() = 0;

        /**
         * Convert the 3DF/HPS camera to generic camera info.
         * @return Generic camera informations.
         */
        virtual CameraInfo getCameraInfo() = 0;

        /**
         * Update segment transformation.
         */
        virtual void updateSelectedSegmentTransform() = 0;

        // Static Methods:
        // ---------------

      private:
        /**
         * Removes the current lighting environment from the scene.
         */
        RED_RC removeCurrentLightingEnvironment();

        /**
         * Create a new camera attached to a window.
         * @param[in] a_window Window on which to attach the new camera.
         * @param[in] a_windowWidth Current window width.
         * @param[in] a_windowHeight Current window height.
         * @param[out] a_outCamera Output camera.
         * @return RED_OK if success, otherwise error code.
         */
        static RED_RC createCamera(RED::Object* a_window, int a_windowWidh, int a_windowHeight, int a_vrlId, RED::Object*& a_outCamera);
    };

    /**
     * Set the Luminate license and check its validity.
     * @param[in] a_license Hoops unified license string.
     * @param[out] a_outLicenseIsActive Output flag indicating whether or not the license is active.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC setLicense(char const* a_license, bool& a_outLicenseIsActive);

    /**
     * Set the soft tracer mode.
     * This method must be called before any window creation and the specified mode cannot be changed later.
     * @param[in] a_mode 0 for hardware-only, 1 for hybrid, 2 for software-only.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC setSoftTracerMode(int a_mode);

    /**
     * Create a new Luminate window.
     * @param[in] a_osHandle OS handler. The HWND on Windows or the X-Window id on Linux/UNIX.
     * @param[in] a_windowWidth Initial window width.
     * @param[in] a_windowHeight Initial window height.
     * @param[in] a_display (Linux/UNIX only) pointer to the X display.
     * @param[in] a_screen (Linux/UNIX only) index of the screen.
     * @param[in] a_visual (Linux/UNIX only) pointer to the X visual.
     * @param[out] a_outWindow Output window.
     * @return RED_OK if success, otherwise error code.
     */
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
     * Remove a scene from a camera.
     * @param[in] a_camera Camera from which to remove the scene root.
     * @param[in] a_sceneInfo Scene description to remove.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC removeSceneFromCamera(RED::Object* a_camera, LuminateSceneInfo const& a_sceneInfo);

    /**
     * Save the current Luminate scene as a .red file.
     * Scene DAG and current camera state will be saved but
     * not the background image.
     * @param[in] a_sceneRootShape Scene root to save.
     * @param[in] a_camera Camera to save.
     * @param[in] a_outputFilepath Output filepath where to write the .red file.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC saveCameraAndSceneAsRedFile(RED::Object* a_sceneRootShape, RED::Object* a_camera, std::string const& a_outputFilepath);

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

    /**
     * Check and perform drawing call in a window using hardware.
     * @param[in] a_window Window where to draw.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC checkDrawHardware(RED::Object* a_window);

    /**
     * Check and store last frame statistics.
     * @param[in] a_window Window where the drawing as occured.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC checkFrameStatistics(RED::Object* a_window, FrameStatistics* a_stats, bool& a_ioFrameIsComplete);

} // namespace hoops_luminate_bridge

#endif
