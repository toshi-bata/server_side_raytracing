#include "AxisTriad.h"

#include <REDObject.h>
#include <REDFactory.h>
#include <REDIViewpoint.h>
#include <REDIResourceManager.h>
#include <REDIOptions.h>
#include <REDIWindow.h>
#include <REDIViewpointRenderList.h>
#include <REDIShape.h>
#include <REDRenderShaderViewport.h>
#include <REDLayerSet.h>
#include <REDITransformShape.h>

#include "LuminateRCTest.h"

namespace HC_luminate_bridge {
    RED_RC createAxisSystemMaterial(const RED::Color& a_color, RED::Object*& a_outMaterial)
    {
        //////////////////////////////////////////
        // Retrieve the resource manager from singleton
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Create a new material.
        //////////////////////////////////////////

        RC_CHECK(iresourceManager->CreateMaterial(a_outMaterial, iresourceManager->GetState()));

        //////////////////////////////////////////
        // Initialize material shaders.
        //////////////////////////////////////////

        RED::IMaterial* imatr = a_outMaterial->As<RED::IMaterial>();

        RED_RC rc;
        RED::RenderShaderViewport rsh(RED::MTL_PRELIT,
                                      RED::RSV_TRIANGLE,
                                      false,
                                      RED::MCL_COLOR,
                                      a_color * 0.85f,
                                      a_color * 0.15f,
                                      RED::Color::WHITE,
                                      10.0f,
                                      RED::Color::WHITE,
                                      RED::Color::WHITE,
                                      resourceManager,
                                      rc);

        RC_CHECK(imatr->RegisterShader(rsh, iresourceManager->GetState()));
        RC_CHECK(imatr->AddShaderToPass(
            rsh.GetID(), RED::MTL_PRELIT, RED::LIST_LAST, RED::LayerSet::ALL_LAYERS, iresourceManager->GetState()));

        return RED_OK;
    }

    RED_RC createAxisTriad(RED::Object* a_window, const int a_num_vrl, AxisTriad& a_outAxisTriad)
    {
        // Retrieve the resource manager from singleton:
        // ---------------------------------------------

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();
        RED::IOptions* ioptions = resourceManager->As<RED::IOptions>();

        // General initializations:
        // ------------------------

        RED::IWindow* iwindow = a_window->As<RED::IWindow>();

        RED::Object* vrl;
        //RC_TEST(iwindow->GetDefaultVRL(vrl));
        RC_TEST(iwindow->GetVRL(vrl, a_num_vrl));
        RED::IViewpointRenderList* ivrl = vrl->As<RED::IViewpointRenderList>();

        int width, height;
        RC_TEST(iwindow->GetSize(width, height));

        // Initialize the axis system:
        // ---------------------------

        // Axis system viewpoint:
        RED::Object* axisCamera = RED::Factory::CreateInstance(CID_REDViewpoint);

        RED::IViewpoint* icamera = axisCamera->As<RED::IViewpoint>();

        RC_TEST(icamera->SetFrustumParallel(70.0, 70.0, iresourceManager->GetState()));
        RC_TEST(icamera->SetEye(RED::Vector3::ZERO, iresourceManager->GetState()));
        RC_TEST(icamera->SetNearFar(-100.0, 100.0, iresourceManager->GetState()));

        RED::IOptions* icamopt = axisCamera->As<RED::IOptions>();
        RC_TEST(ioptions->SetOptionValue(RED::OPTIONS_VIEWPOINT_PICKABLE, false, iresourceManager->GetState()));
        RC_TEST(icamopt->SetOptionValue(RED::OPTIONS_RAY_PRIMARY, false, iresourceManager->GetState())); // Disable rays to render it using GPU

        RC_TEST(ivrl->InsertViewpoint(axisCamera,
                                      RED::VST_FRONT,
                                      RED::LIST_FIRST,
                                      width,
                                      0,
                                      -100,
                                      100,
                                      0.0f,
                                      0.0f,
                                      RED::VSP_ANCHOR_STRETCHED,
                                      RED::VSP_SIZE_FIXED,
                                      iresourceManager->GetState()));

        // Origin sphere:
        RED::Object* sphere = RED::Factory::CreateInstance(CID_REDMeshShape);

        RED::IShape* issphere = sphere->As<RED::IShape>();
        RED::Object* materialWhite;
        RC_TEST(createAxisSystemMaterial(RED::Color::WHITE, materialWhite));
        RC_TEST(issphere->SetMaterial(materialWhite, iresourceManager->GetState()));

        RED::IMeshShape* imsphere = sphere->As<RED::IMeshShape>();
        RC_TEST(imsphere->Sphere(RED::Vector3::ZERO, 10.0f, 20, 20, iresourceManager->GetState()));
        RC_TEST(icamera->AddShape(sphere, iresourceManager->GetState()));

        RED::Object *materialRed, *materialGreen, *materialBlue;
        RC_TEST(createAxisSystemMaterial(RED::Color::RED, materialRed));
        RC_TEST(createAxisSystemMaterial(RED::Color::GREEN, materialGreen));
        RC_TEST(createAxisSystemMaterial(RED::Color::BLUE, materialBlue));

        for (int i = 0; i < 3; i++) {
            // Axis cylinders:
            RED::Object* cylinder = RED::Factory::CreateInstance(CID_REDMeshShape);

            double cyl_height = 50.0;
            RED::IMeshShape* imcylinder = cylinder->As<RED::IMeshShape>();
            RC_TEST(imcylinder->Cylinder(
                RED::Vector3(0.0, 0.0, cyl_height / 2.0), 5.0f, (float)cyl_height / 2.0f, 20, iresourceManager->GetState()));

            // Axis top cones:
            RED::Object* cone = RED::Factory::CreateInstance(CID_REDMeshShape);

            double cone_height = 20.0;
            RED::IMeshShape* imcone = cone->As<RED::IMeshShape>();
            RC_TEST(imcone->Cone(RED::Vector3(0.0, 0.0, cyl_height + cone_height / 2.0),
                                 10.0f,
                                 (float)cone_height / 2.0f,
                                 20,
                                 iresourceManager->GetState()));

            // Transform node:
            RED::Matrix mrot;
            RED::Object* matr;

            // Use same color code than 3DF/HPS

            if (i == 0) {
                RC_TEST(mrot.RotationAxisMatrix(&(RED::Vector3::ZERO._x), &(RED::Vector3::YAXIS._x), RED_PI / 2.0f));
                matr = materialBlue;
            }
            else if (i == 1) {
                RC_TEST(mrot.RotationAxisMatrix(&(RED::Vector3::ZERO._x), &(RED::Vector3::XAXIS._x), -RED_PI / 2.0f));
                matr = materialGreen;
            }
            else {
                mrot = RED::Matrix::IDENTITY;
                matr = materialRed;
            }

            RED::Object* node = RED::Factory::CreateInstance(CID_REDTransformShape);

            RED::IShape* isnode = node->As<RED::IShape>();
            RC_TEST(isnode->SetMaterial(matr, iresourceManager->GetState()));

            RED::ITransformShape* itnode = node->As<RED::ITransformShape>();
            RC_TEST(itnode->SetMatrix(&mrot, iresourceManager->GetState()));
            RC_TEST(itnode->AddChild(cylinder, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState()));
            RC_TEST(itnode->AddChild(cone, RED_SHP_DAG_NO_UPDATE, iresourceManager->GetState()));

            RC_TEST(icamera->AddShape(node, iresourceManager->GetState()));
        }

        a_outAxisTriad.axisCamera = axisCamera;
        a_outAxisTriad.materialWhite = materialWhite;
        a_outAxisTriad.materialRed = materialRed;
        a_outAxisTriad.materialGreen = materialGreen;
        a_outAxisTriad.materialBlue = materialBlue;

        return RED_OK;
    }

    RED_RC synchronizeAxisTriadWithCamera(AxisTriad const& a_axisTriad, RED::Object* a_camera)
    {
        //////////////////////////////////////////
        // Retrieve the resource manager from singleton
        //////////////////////////////////////////

        RED::Object* resourceManager = RED::Factory::CreateInstance(CID_REDResourceManager);
        RED::IResourceManager* iresourceManager = resourceManager->As<RED::IResourceManager>();

        //////////////////////////////////////////
        // Synchronise AxisTriad and camera viewing
        // matrices.
        //////////////////////////////////////////

        RED::IViewpoint* iviewpoint = a_camera->As<RED::IViewpoint>();

        RED::Matrix matx;
        RC_TEST(iviewpoint->GetViewingMatrix(matx));
        matx.SetTranslation(RED::Vector3::ZERO);

        RED::IViewpoint* iaxiscamera = a_axisTriad.axisCamera->As<RED::IViewpoint>();
        RC_TEST(iaxiscamera->SetViewingMatrix(matx, iresourceManager->GetState()));

        return RED_OK;
    }
} // namespace hoops_luminate_bridge
