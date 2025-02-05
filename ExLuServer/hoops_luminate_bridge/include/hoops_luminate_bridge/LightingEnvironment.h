#ifndef LUMINATEBRIDGE_LIGHTINGENVIRONMENT_H
#define LUMINATEBRIDGE_LIGHTINGENVIRONMENT_H

#include <RED.h>
#include <REDIResourceManager.h>
#include <REDIWindow.h>
#include <REDIViewpoint.h>

#include <string>

#ifdef _LIN32
    #ifdef None
        #undef None
    #endif
#endif

namespace hoops_luminate_bridge {

    /**
     * Structure storing data about a default lighting model.
     */
    struct DefaultLightingModel {
        // Light of the environment, linked to the background image.
        RED::Object* skyLight;
        // Background image.
        RED::Object* backgroundCubeImage;
    };

    /**
     * Structure storing data about a physical sun and sky
     * lighting model.
     */
    struct PhysicalSunSkyLightingModel {
        // Light of the sun.
        RED::Object* sunLight;
        // Light of the sky, linked to the background image.
        RED::Object* skyLight;
        // Sky background image.
        RED::Object* backgroundCubeImage;
    };

    /**
     * Structure storing data about an environment map
     * lighting model.
     */
    struct EnvironmentMapLightingModel {
        // Background image.
        RED::Object* backgroundCubeImage;
        // Light of the environment, linked to the backgound image.
        RED::Object* skyLight;
        RED::Object* sunLight;
        // Plain background color if the background image is not visible.
        RED::Color backColor;
        // Whether of not to show the background image.
        bool imageIsVisible;
        // Environment map path.
        RED::String imagePath;
    };

    /*
     * Light shape enum
     */
    enum LightShape { Sphere = 0, Box = 1, Plane = 2 };

    /*
     * Structure storing data about a light.
     */
    struct LuminateLight {
        std::string name;
        double intensity;
        RED::Color color;
        LightShape shape;
        RED::Vector3 position;
        RED::Vector3 size;
        RED::Vector3 rotation;

        RED::Object* light;
        RED::Object* lightTransformation;

        LuminateLight(std::string a_name,
                      double a_intensity,
                      RED::Color a_color,
                      LightShape a_shape,
                      RED::Vector3 a_position,
                      RED::Vector3 a_size,
                      RED::Vector3 a_rotation):
            name(a_name),
            intensity(a_intensity), color(a_color), shape(a_shape), position(a_position), size(a_size), rotation(a_rotation),
            light(nullptr), lightTransformation(nullptr)
        {
        }
    };

    /**
     * Create a new default lighting environment.
     * @param[out] a_outModel Output model data.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC createDefaultModel(DefaultLightingModel& a_outModel);

    /**
     * Create a new lighting environment based on physical sun and sky model.
     * @param[out] a_outModel Output model data.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC createPhysicalSunSkyModel(PhysicalSunSkyLightingModel& a_outModel);

    /**
     * Create a new lighting environment based on an environment map.
     * @param[in] a_imagePath Environment image path to use as sky light and background image.
     * @param[in] a_backColor Plain background color to set if the background image is not set as visible.
     * @param[in] a_visible Whether or not to show the background image.
     * @param[out] a_outModel Output model data.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC createEnvironmentImageLightingModel(const RED::String& a_imagePath,
                                               const RED::Color& a_backColor,
                                               bool a_visible,
                                               EnvironmentMapLightingModel& a_outModel);

    /**
     * Add a default lighting model to a scene.
     * The background image will be attached to the window VRL and the lights to the scene root.
     * @param[in] a_window Window receiving the background image.
     * @param[in] a_transformShape Scene root shape receiving the lights.
     * @param[in] a_model Lighting model to setup.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC addDefaultModel(RED::Object* a_window, const int a_num_vrl, RED::Object* a_transformShape, DefaultLightingModel const& a_model);

    /**
     * Removes a default lighting model from a scene.
     * @param[in] a_window Window owning the background image.
     * @param[in] a_transformShape Scene root shape owning the lights.
     * @param[in] a_model Current lighting model.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC
    removeDefaultModel(RED::Object* a_window, const int a_num_vrl, RED::Object* a_transformShape, DefaultLightingModel const& a_model);

    /**
     * Add a sun/sky lighting model to a scene.
     * The background image will be attached to the window VRL and the lights to the scene root.
     * @param[in] a_window Window receiving the background image.
     * @param[in] a_transformShape Scene root shape receiving the lights.
     * @param[in] a_model Lighting model to setup.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC addSunSkyModel(RED::Object* a_window, const int a_num_vrl, RED::Object* a_transformShape, PhysicalSunSkyLightingModel const& a_model);

    /**
     * Removes a sun/sky lighting model from a scene.
     * @param[in] a_window Window owning the background image.
     * @param[in] a_transformShape Scene root shape owning the lights.
     * @param[in] a_model Current lighting model.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC
    removeSunSkyModel(RED::Object* a_window, const int a_num_vrl, RED::Object* a_transformShape, PhysicalSunSkyLightingModel const& a_model);

    /**
     * Add an environment map lighting model to a scene.
     * The background image will be attached to the window VRL and the lights to the scene root.
     * @param[in] a_window Window receiving the background image.
     * @param[in] a_transformShape Scene root shape receiving the lights.
     * @param[in] a_model Lighting model to setup.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC
    addEnvironmentMapModel(RED::Object* a_window, const int a_num_vrl, RED::Object* a_transformShape, EnvironmentMapLightingModel const& a_model);

    /**
     * Removes an environment map lighting model from a scene.
     * @param[in] a_window Window owning the background image.
     * @param[in] a_transformShape Scene root shape owning the lights.
     * @param[in] a_model Current lighting model.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC
    removeEnvironmentMapModel(RED::Object* a_window, const int a_num_vrl, RED::Object* a_transformShape, EnvironmentMapLightingModel const& a_model);

} // namespace hoops_luminate_bridge

#endif