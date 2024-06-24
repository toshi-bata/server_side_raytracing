#include <cstring>

#include <REDIResourceManager.h>
#include <REDIImage2D.h>
#include <REDLayerSet.h>
#include <REDFactory.h>
#include <REDIREDFile.h>
#include <REDImageTools.h>

#include "LuminateRCTest.h"
#include "ConversionTools.h"

#define PBR_TEXTURE_SIZE 1024

namespace HC_luminate_bridge {
    RED::Object* createREDIImage2D(ImageData const& a_imageData, RED::Object* a_resourceManager)
    {
        RED::IResourceManager* iresourceManager = a_resourceManager->As<RED::IResourceManager>();
        RED::Object* image2d = nullptr;

        // Lets make some asumptions according to bytes per pixels, and for now support only RGB RGBA.

        bool formatIsSupported = true;

        RED::FORMAT redFormat = RED::FORMAT::FMT_FLOAT_RGB;

        switch (a_imageData.bytesPerPixel) {
            case 3:
                redFormat = RED::FMT_RGB;
                break;
            case 4:
                redFormat = RED::FMT_RGBA;
                break;
            default:
                formatIsSupported = false;
                break;
        }

        if (formatIsSupported) {
            // Create associated Red image

            RC_CHECK(iresourceManager->CreateImage2D(image2d, iresourceManager->GetState()));
            RED::IImage2D* redIImage2d = image2d->As<RED::IImage2D>();
            RC_CHECK(redIImage2d->SetLocalPixels(nullptr, redFormat, a_imageData.width, a_imageData.height));

            unsigned char* pixels = redIImage2d->GetLocalPixels();
            std::memcpy(pixels, a_imageData.pixelData, a_imageData.width * a_imageData.height * a_imageData.bytesPerPixel);

            RC_CHECK(redIImage2d->SetPixels(RED::TGT_TEX_2D, iresourceManager->GetState()));
        }

        return image2d;
    }

    RealisticMaterialInfo createDefaultRealisticMaterialInfo()
    {
        RealisticMaterialInfo output;
        output.iorValue = 1.0; // No refraction, like in Visualize.
        output.fresnelActive = false; // No fresnel, like in Visualize.
        output.colorsByChannel[ColorChannels::ReflectionColor] = RED::Color::DARK_GREY;
        output.colorsByChannel[ColorChannels::AnisotropyColor] = RED::Color::DARK_GREY;
        return output;
    }

    RED::Object* getRedImageFromTextureName(std::string const& a_textureName,
                                            ImageNameToLuminateMap const& a_imageNameToLuminateMap,
                                            TextureNameImageNameMap const& a_textureNameImageNameMap)
    {
        TextureNameImageNameMap::const_iterator imageNameIt = a_textureNameImageNameMap.find(a_textureName);
        if (imageNameIt == a_textureNameImageNameMap.end())
            return nullptr;

        ImageNameToLuminateMap::const_iterator redImageIt = a_imageNameToLuminateMap.find(imageNameIt->second);
        if (redImageIt == a_imageNameToLuminateMap.end())
            return nullptr;

        return redImageIt->second;
    }

    RED::Object* createREDMaterial(RealisticMaterialInfo const& a_materialInfo,
                                   RED::Object* a_resourceManager,
                                   ImageNameToLuminateMap const& a_imageNameToLuminateMap,
                                   TextureNameImageNameMap const& a_textureNameImageNameMap,
                                   RealisticMaterialMap& a_ioMaterialMap)
    {
        RED::IResourceManager* iresourceManager = a_resourceManager->As<RED::IResourceManager>();

        // Lookup material map to see if it already exists
        RealisticMaterialMap::iterator materialIt = a_ioMaterialMap.find(a_materialInfo);

        if (materialIt != a_ioMaterialMap.end())
            return materialIt->second;

        RED::Object* material = nullptr;

        iresourceManager->CreateMaterial(material, iresourceManager->GetState());
        RED::IMaterial* imaterial = material->As<RED::IMaterial>();

        // Diffuse

        bool const& fresnelIsActive = a_materialInfo.fresnelActive;
        const RED::Color& diffuseColor = a_materialInfo.colorsByChannel[ColorChannels::DiffuseColor];

        RED::Object* diffuseTexture =
            getRedImageFromTextureName(a_materialInfo.textureNameByChannel[TextureChannels::DiffuseTexture].textureName,
                                       a_imageNameToLuminateMap,
                                       a_textureNameImageNameMap);

        const RED::Matrix& diffuseTextureMatrix =
            a_materialInfo.textureNameByChannel[TextureChannels::DiffuseTexture].textureMatrix;

        // Reflection

        const RED::Color& reflectionColor = a_materialInfo.colorsByChannel[ColorChannels::ReflectionColor];
        RED::Object* reflectionTexture =
            getRedImageFromTextureName(a_materialInfo.textureNameByChannel[TextureChannels::ReflectionTexture].textureName,
                                       a_imageNameToLuminateMap,
                                       a_textureNameImageNameMap);
        const RED::Matrix& reflectionTextureMatrix =
            a_materialInfo.textureNameByChannel[TextureChannels::ReflectionTexture].textureMatrix;

        // Transmission

        const RED::Color& transmissionColor = a_materialInfo.colorsByChannel[ColorChannels::TransmissionColor];
        RED::Object* transmissionTexture =
            getRedImageFromTextureName(a_materialInfo.textureNameByChannel[TextureChannels::TransmissionTexture].textureName,
                                       a_imageNameToLuminateMap,
                                       a_textureNameImageNameMap);
        const RED::Matrix& transmissionTextureMatrix =
            a_materialInfo.textureNameByChannel[TextureChannels::TransmissionTexture].textureMatrix;

        // IOR

        float iorValue = a_materialInfo.iorValue;
        RED::Object* iorTexture =
            getRedImageFromTextureName(a_materialInfo.textureNameByChannel[TextureChannels::IoRTexture].textureName,
                                       a_imageNameToLuminateMap,
                                       a_textureNameImageNameMap);
        const RED::Matrix& iorTextureMatrix = a_materialInfo.textureNameByChannel[TextureChannels::IoRTexture].textureMatrix;

        // Reflection anisotropy

        const RED::Color& anisotropyColor = a_materialInfo.colorsByChannel[ColorChannels::AnisotropyColor];
        RED::Object* anisotropyTexture =
            getRedImageFromTextureName(a_materialInfo.textureNameByChannel[TextureChannels::AnisotropyTexture].textureName,
                                       a_imageNameToLuminateMap,
                                       a_textureNameImageNameMap);
        const RED::Matrix& anisotropyTextureMatrix =
            a_materialInfo.textureNameByChannel[TextureChannels::AnisotropyTexture].textureMatrix;

        // Bump

        RED::Object* bumpTexture =
            getRedImageFromTextureName(a_materialInfo.textureNameByChannel[TextureChannels::BumpTexture].textureName,
                                       a_imageNameToLuminateMap,
                                       a_textureNameImageNameMap);
        const RED::Matrix& bumpTextureMatrix = a_materialInfo.textureNameByChannel[TextureChannels::BumpTexture].textureMatrix;

        RED::MESH_CHANNEL bumpTextureChannel = RED::MESH_CHANNEL::MCL_TEX0;
        if (a_materialInfo.textureNameByChannel[TextureChannels::BumpTexture].textureParameterization !=
            a_materialInfo.textureNameByChannel[TextureChannels::DiffuseTexture].textureParameterization)
            bumpTextureChannel = RED::MESH_CHANNEL::MCL_TEX1;

        // Material creation

        /* clang-format off */
        RC_CHECK(imaterial->SetupRealisticMaterial(
            true, fresnelIsActive,                                                                          // Double sided, Fresnel
            diffuseColor, diffuseTexture, diffuseTextureMatrix, RED::MCL_TEX0,                              // Diffusion
            reflectionColor, reflectionTexture, reflectionTextureMatrix, RED::MCL_TEX0,                     // Reflection
            RED::Color::BLACK, 0.0f,                                                                        // Reflection fog
            false, false, nullptr, RED::Matrix::IDENTITY,                                                   // Environment
            transmissionColor, transmissionTexture, transmissionTextureMatrix, RED::MCL_TEX0,               // Transmission
            0.0f, nullptr, RED::Matrix::IDENTITY, RED::MCL_TEX0,                                            // Transmission glossiness
            iorValue, iorTexture, iorTextureMatrix, RED::MCL_TEX0,                                          // IOR
            RED::Color::WHITE, 1.0f,                                                                        // Transmission scattering
            false, false,                                                                                   // Caustics
            anisotropyColor, anisotropyTexture, anisotropyTextureMatrix, RED::MCL_TEX0,                     // Reflection anisotropy
            0.0f, nullptr, RED::Matrix::IDENTITY, RED::MCL_TEX0,                                            // Reflection anisotropy orientation
            bumpTexture, bumpTextureMatrix,bumpTextureChannel, RED::MCL_USER0,                              // Bump
            0.0f, 0.0f, nullptr, RED::Matrix::IDENTITY, RED::MCL_TEX0,                                      // Displacement
            nullptr, &RED::LayerSet::ALL_LAYERS,                                                            // Layersets
            a_resourceManager, iresourceManager->GetState()));                                              // Engine stuff
        /* clang-format on */

        // Memorize the material
        a_ioMaterialMap[a_materialInfo] = material;

        return material;
    }

    RED::Object* createPolygonREDMeshShape(const RED::State& a_state, int a_pointslength, float* a_points)
    {
        RED::Object* result = RED::Factory::CreateInstance(CID_REDMeshShape);
        RED::IMeshShape* imesh = result->As<RED::IMeshShape>();
        RED::Vector<float> poly_points;
        RED::Vector<RED::Vector<float>> vectors_points;

        for (int index_point = 0; index_point < a_pointslength * 3;) {
            poly_points.push_back(a_points[index_point++]);
            poly_points.push_back(a_points[index_point++]);
            poly_points.push_back(a_points[index_point++]);
        }
        vectors_points.push_back(poly_points);

        RC_CHECK(imesh->Polygon(vectors_points, a_state));

        return result;
    }

    RED_RC setVertexNormals(const RED::State& a_state, RED::IMeshShape* a_dstShape, int a_nbVertices, float* a_normals)
    {
        if (a_normals == nullptr) {
            // No normal to set...
            // NOTE: normals could be automatically generated using Luminate :
            // RC_TEST(imesh->Shade(RED::MCL_NORMAL, RED::MFT_FLOAT, RED::MCL_VERTEX, state));
            return RED_OK;
        }

        RC_TEST(a_dstShape->SetArray(RED::MCL_NORMAL, a_normals, a_nbVertices, 3, RED::MFT_FLOAT, a_state));

        return RED_OK;
    }

    bool needToReverseFaces(float* a_points, float* a_normals, int a_indiceA, int a_indiceB, int a_indiceC)
    {
        float limit = 0.f;

        // Get correct array indice for each face
        a_indiceA = a_indiceA * 3;
        a_indiceB = a_indiceB * 3;
        a_indiceC = a_indiceC * 3;

        const RED::Vector3 pa(a_points[a_indiceA], a_points[a_indiceA + 1], a_points[a_indiceA + 2]);
        const RED::Vector3 pb(a_points[a_indiceB], a_points[a_indiceB + 1], a_points[a_indiceB + 2]);
        const RED::Vector3 pc(a_points[a_indiceC], a_points[a_indiceC + 1], a_points[a_indiceC + 2]);
        RED::Vector3 triNormal = (pb - pa).Cross2(pc - pa);
        triNormal.Normalize();

        const RED::Vector3 vna(a_normals[a_indiceA], a_normals[a_indiceA + 1], a_normals[a_indiceA + 2]);
        const RED::Vector3 vnb(a_normals[a_indiceB], a_normals[a_indiceB + 1], a_normals[a_indiceB + 2]);
        const RED::Vector3 vnc(a_normals[a_indiceC], a_normals[a_indiceC + 1], a_normals[a_indiceC + 2]);

        return (vna.Dot(triNormal) <= limit && vnb.Dot(triNormal) <= limit && vnc.Dot(triNormal) <= limit);
    }

    RED::Object* createREDMeshShapeFromTriStrips(const RED::State& a_state,
                                                 int a_tristrips_length,
                                                 int* a_tristrips,
                                                 int a_pointslength,
                                                 float* a_points,
                                                 float* a_normals)
    {
        RED::Object* result = RED::Factory::CreateInstance(CID_REDMeshShape);
        RED::IMeshShape* imesh = result->As<RED::IMeshShape>();

        RC_CHECK(imesh->SetArray(RED::MCL_VERTEX, a_points, a_pointslength, 3, RED::MFT_FLOAT, a_state));

        int triangleCount = 0;
        RED::Vector<int> indices;

        // Decode tristrips into regular triangle indices
        int* strips_it = a_tristrips;

        // Walk through faces
        while (strips_it < a_tristrips + a_tristrips_length) {
            // Recover face triangles length
            int stripLength = *strips_it++;

            // Walk through the current face
            for (int i = 0; i < stripLength - 2; i++) {
                int a, b, c;
                if (i % 2) {
                    a = strips_it[1];
                    b = strips_it[0];
                    c = strips_it[2];
                }
                else {
                    a = strips_it[0];
                    b = strips_it[1];
                    c = strips_it[2];
                }

                if (a_normals != nullptr && needToReverseFaces(a_points, a_normals, a, b, c)) {
                    // Reverse the triangle orientation to match the vertex normals, otherwise Luminate is lost
                    // when computing path tracing
                    indices.push_back(c);
                    indices.push_back(b);
                    indices.push_back(a);
                }
                else {
                    indices.push_back(a);
                    indices.push_back(b);
                    indices.push_back(c);
                }

                triangleCount++;
                strips_it++;
            }
            strips_it += 2;
        }

        RC_CHECK(imesh->AddTriangles(&indices[0], triangleCount, a_state));

        return result;
    }

    PBRToRealisticMaterialInput createPBRToRealisticMaterialInput(const RED::Color& a_baseColorFactor,
                                                                  std::string const& a_albedoTextureName,
                                                                  std::string const& a_roughnessTextureName,
                                                                  std::string const& a_metalnessTextureName,
                                                                  std::string const& a_occlusionTextureName,
                                                                  std::string const& a_normalTextureName,
                                                                  float a_roughnessFactor,
                                                                  float a_metalnessFactor,
                                                                  float a_occlusionFactor,
                                                                  float a_alphaFactor)
    {
        PBRToRealisticMaterialInput pbrToRealisticMaterialInput;

        pbrToRealisticMaterialInput.diffuseColor = a_baseColorFactor;
        pbrToRealisticMaterialInput.textureNameByChannel[PBRTextureChannels::Diffuse] = a_albedoTextureName;
        pbrToRealisticMaterialInput.textureNameByChannel[PBRTextureChannels::Roughness] = a_roughnessTextureName;
        pbrToRealisticMaterialInput.textureNameByChannel[PBRTextureChannels::Metalness] = a_metalnessTextureName;
        pbrToRealisticMaterialInput.textureNameByChannel[PBRTextureChannels::AmbientOcclusion] = a_occlusionTextureName;
        pbrToRealisticMaterialInput.textureNameByChannel[PBRTextureChannels::Normal] = a_normalTextureName;
        pbrToRealisticMaterialInput.valueByChannel[PBRValueChannels::RoughnessFactor] = a_roughnessFactor;
        pbrToRealisticMaterialInput.valueByChannel[PBRValueChannels::MetalnessFactor] = a_metalnessFactor;
        pbrToRealisticMaterialInput.valueByChannel[PBRValueChannels::OcclusionFactor] = a_occlusionFactor;
        pbrToRealisticMaterialInput.valueByChannel[PBRValueChannels::AlphaFactor] = a_alphaFactor;

        return pbrToRealisticMaterialInput;
    }

    bool retrievePBRToRealisticMaterialConversion(PBRToRealisticMaterialInput const& a_conversionInput,
                                                  PBRToRealisticConversionMap const& a_ioPBRToRealisticConversionMap,
                                                  RealisticMaterialInfo& a_ioMaterialInfo)
    {
        PBRToRealisticConversionMap::const_iterator conversionIt = a_ioPBRToRealisticConversionMap.find(a_conversionInput);
        bool found = false;

        if (conversionIt != a_ioPBRToRealisticConversionMap.end()) {
            a_ioMaterialInfo.textureNameByChannel[TextureChannels::DiffuseTexture].textureName =
                conversionIt->second.diffuseTextureName;
            a_ioMaterialInfo.textureNameByChannel[TextureChannels::ReflectionTexture].textureName =
                conversionIt->second.reflectionTextureName;
            a_ioMaterialInfo.textureNameByChannel[TextureChannels::AnisotropyTexture].textureName =
                conversionIt->second.anisotropyTextureName;
            a_ioMaterialInfo.textureNameByChannel[TextureChannels::IoRTexture].textureName = conversionIt->second.iorTextureName;
            a_ioMaterialInfo.textureNameByChannel[TextureChannels::BumpTexture].textureName =
                conversionIt->second.normalTextureName;

            found = true;
        }

        return found;
    }

    void convertPBRToRealisticMaterial(RED::Object* a_resourceManager,
                                       std::string const& a_textureBaseName,
                                       RED::Object* a_albedoImage,
                                       RED::Object* a_roughnessImage,
                                       RED::Object* a_metalnessImage,
                                       RED::Object* a_ambientOcclusionImage,
                                       std::string const& a_normalTextureName,
                                       PBRToRealisticMaterialInput const& a_conversionInput,
                                       ImageNameToLuminateMap& a_ioImageNameToLuminateMap,
                                       TextureNameImageNameMap& a_ioTextureNameImageNameMap,
                                       PBRToRealisticConversionMap& a_ioPBRToRealisticConversionMap,
                                       RealisticMaterialInfo& a_ioMaterialInfo)
    {
        RED::IResourceManager* iresourceManager = a_resourceManager->As<RED::IResourceManager>();

        // Create realistic material output textures.

        RED::Object *outputDiffuseImage, *outputReflectionImage, *outputAnisotropyImage, *outputIoRImage;

        RC_CHECK(iresourceManager->CreateImage2D(outputDiffuseImage, iresourceManager->GetState()));
        RC_CHECK(iresourceManager->CreateImage2D(outputReflectionImage, iresourceManager->GetState()));
        RC_CHECK(iresourceManager->CreateImage2D(outputAnisotropyImage, iresourceManager->GetState()));
        RC_CHECK(iresourceManager->CreateImage2D(outputIoRImage, iresourceManager->GetState()));

        // Perform the conversion.

        /* clang-format off */
        RED::ImageTools::ConvertMetallicToRealisticMaterial(
            outputDiffuseImage, PBR_TEXTURE_SIZE, PBR_TEXTURE_SIZE,                                         // Returned diffuse
            outputReflectionImage, PBR_TEXTURE_SIZE, PBR_TEXTURE_SIZE,                                      // Returned reflection
            outputAnisotropyImage, PBR_TEXTURE_SIZE, PBR_TEXTURE_SIZE,                                      // Returned anisotropy
            outputIoRImage, PBR_TEXTURE_SIZE, PBR_TEXTURE_SIZE,                                             // Returned IOR
            nullptr, 0,                                                                                     // Env map
            a_albedoImage, a_conversionInput.diffuseColor,                                                  // PBR Albedo
            a_roughnessImage, a_conversionInput.valueByChannel[PBRValueChannels::RoughnessFactor],          // PBR roughness
            a_metalnessImage, a_conversionInput.valueByChannel[PBRValueChannels::MetalnessFactor],          // PBR metallic
            a_ambientOcclusionImage, a_conversionInput.valueByChannel[PBRValueChannels::OcclusionFactor],   // PBR ambient occlusion
            nullptr, a_conversionInput.valueByChannel[PBRValueChannels::AlphaFactor], false,                // Alpha
            nullptr,                                                                                        // Env text
            a_resourceManager,
            iresourceManager->GetState());
        /* clang-format on */

        // Copy local image data to GPU.

        RED::IImage2D* idiffuseImage = outputDiffuseImage->As<RED::IImage2D>();
        RED::IImage2D* ireflectionImage = outputReflectionImage->As<RED::IImage2D>();
        RED::IImage2D* ianisotropyImage = outputAnisotropyImage->As<RED::IImage2D>();
        RED::IImage2D* iIoRImage = outputIoRImage->As<RED::IImage2D>();

        RC_CHECK(idiffuseImage->SetPixels(RED::TGT_TEX_2D, iresourceManager->GetState()));
        RC_CHECK(ireflectionImage->SetPixels(RED::TGT_TEX_2D, iresourceManager->GetState()));
        RC_CHECK(ianisotropyImage->SetPixels(RED::TGT_TEX_2D, iresourceManager->GetState()));
        RC_CHECK(iIoRImage->SetPixels(RED::TGT_TEX_2D, iresourceManager->GetState()));

        // Cache the converted image using unique image and texture names based on segment key.

        std::string diffuseImageName = a_textureBaseName + "_converted_diffuse_image";
        std::string reflectionImageName = a_textureBaseName + "_converted_reflection_image";
        std::string anisotropyImageName = a_textureBaseName + "_converted_anisotropy_image";
        std::string iorImageName = a_textureBaseName + "_converted_ior_image";

        std::string diffuseTextureName = a_textureBaseName + "_converted_diffuse_texture";
        std::string reflectionTextureName = a_textureBaseName + "_converted_reflection_texture";
        std::string anisotropyTextureName = a_textureBaseName + "_converted_anisotropy_texture";
        std::string iorTextureName = a_textureBaseName + "_converted_ior_texture";

        a_ioImageNameToLuminateMap[diffuseImageName] = outputDiffuseImage;
        a_ioImageNameToLuminateMap[reflectionTextureName] = outputReflectionImage;
        a_ioImageNameToLuminateMap[anisotropyTextureName] = outputAnisotropyImage;
        a_ioImageNameToLuminateMap[iorTextureName] = outputIoRImage;

        a_ioTextureNameImageNameMap[diffuseTextureName] = diffuseImageName;
        a_ioTextureNameImageNameMap[reflectionTextureName] = reflectionImageName;
        a_ioTextureNameImageNameMap[anisotropyTextureName] = anisotropyImageName;
        a_ioTextureNameImageNameMap[iorTextureName] = iorImageName;

        // Now that our textures are cached, we can reference them inside the material info.

        a_ioMaterialInfo.textureNameByChannel[TextureChannels::DiffuseTexture].textureName = diffuseTextureName;
        a_ioMaterialInfo.textureNameByChannel[TextureChannels::ReflectionTexture].textureName = reflectionTextureName;
        a_ioMaterialInfo.textureNameByChannel[TextureChannels::AnisotropyTexture].textureName = anisotropyTextureName;
        a_ioMaterialInfo.textureNameByChannel[TextureChannels::IoRTexture].textureName = iorTextureName;
        a_ioMaterialInfo.textureNameByChannel[TextureChannels::BumpTexture].textureName = a_normalTextureName;

        // Cache the conversion result.

        PBRToRealisticMaterialOutput conversionOutput;
        conversionOutput.diffuseTextureName = diffuseTextureName;
        conversionOutput.reflectionTextureName = reflectionTextureName;
        conversionOutput.anisotropyTextureName = anisotropyTextureName;
        conversionOutput.iorTextureName = iorTextureName;
        conversionOutput.normalTextureName = a_normalTextureName;
        a_ioPBRToRealisticConversionMap[a_conversionInput] = conversionOutput;
    }
} // namespace HC_luminate_bridge