#include "LightingEnvironment.h"

#include <REDFactory.h>
#include <REDILightShape.h>
#include <REDISkyLightShape.h>
#include <REDIImage2D.h>
#include <REDIImageCube.h>
#include <REDIViewpointRenderList.h>
#include <REDImageTools.h>
#include <REDITransformShape.h>
#include <REDIShape.h>

//#include <hoops_luminate_bridge/HoopsLuminateBridge.h>
#include "LuminateRCTest.h"

#define _USE_MATH_DEFINES
#include <math.h>

namespace HC_luminate_bridge {
    RED_RC createBackgroundCube(RED::Object* a_backgroundTexture, int a_size, RED::Object*& a_outBackgroundCubeImage)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        // Retrieve info about the texture.
        RED::IImage2D* iback = a_backgroundTexture->As<RED::IImage2D>();

        // Needed to retrieve data from GPU
        RC_TEST(iback->GetPixels());

        RED::FORMAT pix_format = iback->GetLocalFormat();
        int pix_width, pix_height;
        iback->GetLocalSize(pix_width, pix_height);
        unsigned char* pixels = iback->GetLocalPixels();

        // create a cube map from the texture

        RC_TEST(iresourceManager->CreateImageCube(a_outBackgroundCubeImage, iresourceManager->GetState()));

        RED::IImageCube* icube = a_outBackgroundCubeImage->As<RED::IImageCube>();
        RC_TEST(icube->CreateEnvironmentMap(RED::FMT_FLOAT_RGB,
                                            RED::ENV_SPHERICAL,
                                            a_size,
                                            pixels,
                                            pix_width,
                                            pix_height,
                                            pix_format,
                                            RED::WM_CLAMP_TO_BORDER,
                                            RED::WM_CLAMP_TO_BORDER,
                                            RED::Color::BLACK,
                                            RED::Matrix::IDENTITY,
                                            RED::Matrix::IDENTITY,
                                            iresourceManager->GetState()));

        return RED_OK;
    }

    RED_RC createDefaultModel(DefaultLightingModel& a_outModel)
    { 
        // Get the resource manager singleton.
        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        // init grid variables;
        int dist = 128;
        unsigned int gSize = 512;
        float lightIntensity = 255.0f;

        // init temp variables
        double u, v, longitude, latitude, phi, theta, x, y, z, maxAbs, colorCoef;

        // create lat long background image
        unsigned int gHeight = 2048;
        unsigned int gWidth = 2 * gHeight;
        RED::Object* latLongLight_hdr;
        RC_TEST(iresourceManager->CreateImage2D(latLongLight_hdr, iresourceManager->GetState()));
        RED::IImage2D* ilatLongLight2D = latLongLight_hdr->As<RED::IImage2D>();
        RED::IImage* ilatLongLight = latLongLight_hdr->As<RED::IImage>();
        ilatLongLight2D->SetLocalPixels(NULL, RED::FMT_FLOAT_RGB, gWidth, gHeight);
        float* llatLongLightPix = (float*)ilatLongLight2D->GetLocalPixels();

        // foreach pixel
        for (unsigned int j = 0; j < gWidth; j++) {
            for (unsigned int i = 0; i < gHeight; i++) {
                // compute uv
                u = j / (double)gWidth;
                v = i / (double)gHeight;
                //// compute lat long
                longitude = 2.0 * M_PI * (u - 0.5);
                latitude = M_PI * (v - 0.5);
                //// compute xyz on sphere
                phi = (M_PI / 2.0 - latitude);
                theta = (M_PI - longitude);
                x = sin(phi) * cos(theta);
                y = cos(phi);
                z = sin(phi) * sin(theta);
                //// compute xyz on cube
                maxAbs = max(max(abs(x), abs(y)), abs(z));
                x /= maxAbs;
                y /= maxAbs;
                z /= maxAbs;
                //// compute uv on face
                if (abs(abs(x) - 1.0) < 1e-9) {
                    u = (y * 0.5) + 0.5;
                    v = (z * 0.5) + 0.5;
                }
                else if (abs(abs(y) - 1.0) < 1e-9) {
                    u = (x * 0.5) + 0.5;
                    v = (z * 0.5) + 0.5;
                }
                else if (abs(abs(z) - 1.0) < 1e-9) {
                    u = (x * 0.5) + 0.5;
                    v = (y * 0.5) + 0.5;
                }

                //// compute color
                colorCoef = 0.85 + 0.15 * (1.0 - pow(sin(u * M_PI * gSize / (double)dist), 2.0)) *
                                       (1.0 - pow(sin(v * M_PI * gSize / (double)dist), 2.0));
                colorCoef *= 0.75 + 0.25 * (1.0 - pow(sin(u * M_PI * gSize / (double)dist), 1500.0)) *
                                        (1.0 - pow(sin(v * M_PI * gSize / (double)dist), 1500.0));
                colorCoef *= (y + 1) * 0.25 + 0.5;

                llatLongLightPix[(3 * j + 0) + (3 * i * gWidth)] = (lightIntensity * colorCoef);
                llatLongLightPix[(3 * j + 1) + (3 * i * gWidth)] = (lightIntensity * colorCoef);
                llatLongLightPix[(3 * j + 2) + (3 * i * gWidth)] = (lightIntensity * colorCoef);
            }
        }
        RC_TEST(ilatLongLight2D->SetPixels(RED::TGT_TEX_RECT, iresourceManager->GetState()));
        RC_TEST(ilatLongLight->SetWrapModes(RED::WM_CLAMP_TO_BORDER, RED::WM_CLAMP_TO_BORDER, iresourceManager->GetState()));

        RED::Object* backgroundCubeImage;
        createBackgroundCube(latLongLight_hdr, 1024, backgroundCubeImage);

        // Create the sky light.
        RED::Object* _sky = RED::Factory::CreateInstance(CID_REDLightShape);
        if (_sky == nullptr)
            RC_TEST(RED_ALLOC_FAILURE);

        // Create the physical sky texture used for illumination.
        RED::ISkyLightShape* isky = _sky->As<RED::ISkyLightShape>();
        RC_TEST(
            isky->SetSkyTexture(latLongLight_hdr, true, RED::Vector3::XAXIS, RED::Vector3::ZAXIS, iresourceManager->GetState()));
        RED::ILightShape* iskylight = _sky->As<RED::ILightShape>();
        RC_TEST(iskylight->SetRenderMode(RED::RM_SHADOW_CASTER, 1, iresourceManager->GetState()));

        a_outModel.backgroundCubeImage = backgroundCubeImage;
        a_outModel.skyLight = _sky;

        return RED_OK;
    }

    RED_RC createPhysicalSunSkyModel(PhysicalSunSkyLightingModel& a_outModel)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        // Create the sky light.
        RED::Object* sky = RED::Factory::CreateInstance(CID_REDLightShape);
        if (sky == nullptr)
            RC_TEST(RED_ALLOC_FAILURE);

        // Turn on the sky shadows.
        RED::ILightShape* ilight = sky->As<RED::ILightShape>();
        RC_TEST(ilight->SetRenderMode(RED::RM_SHADOW_CASTER, 1, iresourceManager->GetState()));

        // Create the physical sky texture used for illumination.
        RED::ISkyLightShape* isky = sky->As<RED::ISkyLightShape>();

        RED::Object* sky_tex = nullptr;

        const RED::Vector3 sun_dir(-0.4472f, 0.0f, 0.8944f);

        RC_TEST(isky->SetPhysicalModel(
            1.0, 1.0, 1.0, 10, 0.3, 0.8, 0.7, sun_dir, 1.0, 1.0, sun_dir, 0.0, 0.0, 0.0, 1.0, iresourceManager->GetState()));

        RC_TEST(isky->CreatePhysicalSkyTexture(sky_tex, false, 256, true, false, true, iresourceManager->GetState()));

        // Use the texture to setup the sky light.
        RC_TEST(isky->SetSkyTexture(sky_tex, true, RED::Vector3::XAXIS, RED::Vector3::ZAXIS, iresourceManager->GetState()));

        // Create the physical sky texture used as window background.
        RED::Object* bg_tex = nullptr;
        RC_TEST(isky->CreatePhysicalSkyTexture(bg_tex, false, 512, false, false, true, iresourceManager->GetState()));

        RED::Object* backgroundCubeImage;
        RC_TEST(createBackgroundCube(bg_tex, 512, backgroundCubeImage));

        // Create the sun light.
        RED::Object* sun = nullptr;
        RC_TEST(isky->SetSunLight(sun, iresourceManager->GetState()));
        RED::ILightShape* isun = sun->As<RED::ILightShape>();
        RC_TEST(isun->SetRenderMode(RED::RM_SHADOW_CASTER, 1, iresourceManager->GetState()));

        a_outModel.sunLight = sun;
        a_outModel.skyLight = sky;
        a_outModel.backgroundCubeImage = backgroundCubeImage;

        return RED_OK;
    }

    RED_RC createEnvironmentImageLightingModel(const RED::String& a_imagePath,
                                               const RED::Color& a_backColor,
                                               bool a_visible,
                                               EnvironmentMapLightingModel& a_outModel)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        RED::Object* sky_hdr;
        RC_TEST(iresourceManager->CreateImage2D(sky_hdr, iresourceManager->GetState()));

        // We load the image in the GPU
        RC_TEST(RED::ImageTools::Load(
            sky_hdr, a_imagePath, RED::FMT_FLOAT_RGB, true, false, RED::TGT_TEX_RECT, iresourceManager->GetState()));

        RED::Object* backgroundCubeImage;
        createBackgroundCube(sky_hdr, 1024, backgroundCubeImage);

        // Create the sky light.
        RED::Object* _sky = RED::Factory::CreateInstance(CID_REDLightShape);
        if (_sky == nullptr)
            RC_TEST(RED_ALLOC_FAILURE);

        // Create the physical sky texture used for illumination.
        RED::ISkyLightShape* isky = _sky->As<RED::ISkyLightShape>();
        RC_TEST(isky->SetSkyTexture(sky_hdr, false, RED::Vector3::XAXIS, RED::Vector3::ZAXIS, iresourceManager->GetState()));
        RED::ILightShape* iskylight = _sky->As<RED::ILightShape>();
        RC_TEST(iskylight->SetRenderMode(RED::RM_SHADOW_CASTER, 1, iresourceManager->GetState()));

        a_outModel.backColor = a_backColor;
        a_outModel.backgroundCubeImage = backgroundCubeImage;
        a_outModel.skyLight = _sky;
        a_outModel.imageIsVisible = a_visible;
        a_outModel.imagePath = a_imagePath;

        return RED_OK;
    }

    RED_RC setWindowBackgroundImage(RED::Object* a_window,
                                    RED::Object* a_backgroundImage,
                                    bool a_visible,
                                    const RED::Color& a_backgroundColor)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Set the background image to default VRL.
        // We only define the cube image here.
        //////////////////////////////////////////

        RED::IWindow* iwindow = a_window->As<RED::IWindow>();

        RED::Object* vrl;
        RC_TEST(iwindow->GetDefaultVRL(vrl));

        RED::IViewpointRenderList* ivrl = vrl->As<RED::IViewpointRenderList>();

        RC_TEST(ivrl->SetBackgroundImages(a_backgroundImage,
                                          RED::Matrix::IDENTITY,
                                          nullptr,
                                          RED::Matrix::IDENTITY,
                                          a_visible,
                                          1.0,
                                          1.0,
                                          iresourceManager->GetState()));

        //////////////////////////////////////////
        // Set the clear color in case that the images
        // are not set to visible.
        //////////////////////////////////////////

        RC_TEST(ivrl->SetClearColor(a_backgroundColor, iresourceManager->GetState()));

        return RED_OK;
    }

    RED_RC addDefaultModel(RED::Object* a_window, RED::Object* a_transformShape, DefaultLightingModel const& a_model)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Set the window background image and back color.
        //////////////////////////////////////////

        setWindowBackgroundImage(a_window, a_model.backgroundCubeImage, true, RED::Color::WHITE);

        //////////////////////////////////////////
        // Set the sky light as child
        //////////////////////////////////////////

        RED::ITransformShape* itransformShape = a_transformShape->As<RED::ITransformShape>();
        RC_TEST(itransformShape->AddChild(a_model.skyLight, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState()));
        return RED_RC();
    }

    RED_RC removeDefaultModel(RED::Object* a_window, RED::Object* a_transformShape, DefaultLightingModel const& a_model)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Removes background image from the scene
        //////////////////////////////////////////

        setWindowBackgroundImage(a_window, nullptr, false, RED::Color::WHITE);

        //////////////////////////////////////////
        // Remove the light
        //////////////////////////////////////////

        RED::ITransformShape* itransformShape = a_transformShape->As<RED::ITransformShape>();
        itransformShape->RemoveChild(a_model.skyLight, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState());
        return RED_RC();
    }

    RED_RC addSunSkyModel(RED::Object* a_window, RED::Object* a_transformShape, PhysicalSunSkyLightingModel const& a_model)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Set the window background image as sky.
        // As we want the sky image visible, we can
        // ignore the background color.
        //////////////////////////////////////////

        setWindowBackgroundImage(a_window, a_model.backgroundCubeImage, true, RED::Color::WHITE);

        //////////////////////////////////////////
        // Set the sky/sun lights as children.
        //////////////////////////////////////////

        RED::ITransformShape* itransformShape = a_transformShape->As<RED::ITransformShape>();
        RC_TEST(itransformShape->AddChild(a_model.skyLight, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState()));
        RC_TEST(itransformShape->AddChild(a_model.sunLight, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState()));

        return RED_OK;
    }

    RED_RC removeSunSkyModel(RED::Object* a_window, RED::Object* a_transformShape, PhysicalSunSkyLightingModel const& a_model)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Removes background image from the scene
        //////////////////////////////////////////

        setWindowBackgroundImage(a_window, nullptr, true, RED::Color::WHITE);

        //////////////////////////////////////////
        // Remove the lights
        //////////////////////////////////////////

        RED::ITransformShape* itransformShape = a_transformShape->As<RED::ITransformShape>();
        itransformShape->RemoveChild(a_model.skyLight, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState());
        itransformShape->RemoveChild(a_model.sunLight, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState());

        return RED_OK;
    }

    RED_RC
    addEnvironmentMapModel(RED::Object* a_window, RED::Object* a_transformShape, EnvironmentMapLightingModel const& a_model)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Set the window background image and back color.
        //////////////////////////////////////////

        setWindowBackgroundImage(a_window, a_model.backgroundCubeImage, a_model.imageIsVisible, a_model.backColor);

        //////////////////////////////////////////
        // Set the sky light as child
        //////////////////////////////////////////

        RED::ITransformShape* itransformShape = a_transformShape->As<RED::ITransformShape>();
        RC_TEST(itransformShape->AddChild(a_model.skyLight, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState()));

        return RED_OK;
    }

    RED_RC
    removeEnvironmentMapModel(RED::Object* a_window, RED::Object* a_transformShape, EnvironmentMapLightingModel const& a_model)
    {
        //////////////////////////////////////////
        // Get the resource manager singleton.
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Removes background image from the scene
        //////////////////////////////////////////

        setWindowBackgroundImage(a_window, nullptr, false, RED::Color::WHITE);

        //////////////////////////////////////////
        // Remove the light
        //////////////////////////////////////////

        RED::ITransformShape* itransformShape = a_transformShape->As<RED::ITransformShape>();
        itransformShape->RemoveChild(a_model.skyLight, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState());

        return RED_OK;
    }
} // namespace HC_luminate_bridge
