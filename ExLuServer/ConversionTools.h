#ifndef LUMINATEBRIDGE_CONVERSIONTOOLS_H
#define LUMINATEBRIDGE_CONVERSIONTOOLS_H

#include <string>
#include <map>
#include <memory>
#include <vector>
#include <REDObject.h>
#include <REDColor.h>
#include <REDMatrix.h>
#include <REDState.h>
#include <REDIMeshShape.h>

namespace HC_luminate_bridge {
    /**
     * Enumeration of different handedness handled by 3DF/HPS.
     */
    enum class Handedness { LeftHanded, RightHanded };

    /**
     * Enumeration of different projection modes handled by 3DF/HPS.
     */
    enum class ProjectionMode { Perspective, Orthographic, Stretched };

    /**
     * Luminate realistic material optional texture inputs
     */
    enum TextureChannels {
        DiffuseTexture = 0,
        ReflectionTexture = 1,
        TransmissionTexture = 2,
        AnisotropyTexture = 3,
        IoRTexture = 4,
        BumpTexture = 5
    };

    /**
     * Luminate realistic material optional color inputs.
     */
    enum ColorChannels { DiffuseColor = 0, ReflectionColor = 1, TransmissionColor = 2, AnisotropyColor = 3 };

    /**
     * Luminate texture coordinates parameterization.
     */
    enum class UVParameterization {
        UV = 0,
        Cylinder = 1,
        NaturalUV = 2,
        Object = 3,
        PhysicalReflection = 4,
        ReflectionVector = 5,
        SurfaceNormal = 6,
        Sphere = 7,
        World = 8
    };

    /**
     * Luminate realistic material texture channel information.
     * Contains the name of the texture used to retrieve the associated
     * RED image and the associated texture matrix to apply.
     */
    struct TextureChannelInfo {
        std::string textureName = "";
        RED::Matrix textureMatrix = RED::Matrix::IDENTITY;
        UVParameterization textureParameterization = UVParameterization::UV;

        bool operator==(TextureChannelInfo const& a_other) const
        {
            return textureName == a_other.textureName && textureMatrix == a_other.textureMatrix &&
                   textureParameterization == a_other.textureParameterization;
        }

        bool operator<(TextureChannelInfo const& a_other) const
        {
            // TODO : support texture matrix < operator.
            return textureName < a_other.textureName;
        }
    };

    /**
     * Hoops Luminate realistic material definition used to create the material itself.
     * If a texture for the same channel than a color is defined, the color is ignored.
     */
    struct RealisticMaterialInfo {
        // Texture data associated to texture channels, indexed by the channel identifier.
        std::vector<TextureChannelInfo> textureNameByChannel = std::vector<TextureChannelInfo>(6);

        // Colors associated to color channels, indexed by the channel identifier.
        std::vector<RED::Color> colorsByChannel = std::vector<RED::Color>(4, RED::Color::BLACK);

        // Individual parameters value.
        float iorValue = 1.0;
        bool fresnelActive = true;
    };

    struct RealisticMaterialInfoComparator {
        bool operator()(RealisticMaterialInfo const& lhs, RealisticMaterialInfo const& rhs) const
        {
            if (lhs.textureNameByChannel == rhs.textureNameByChannel) {
                if (lhs.colorsByChannel == rhs.colorsByChannel) {
                    if (lhs.iorValue == rhs.iorValue) {
                        return lhs.fresnelActive < rhs.fresnelActive;
                    }
                    else {
                        return lhs.iorValue < rhs.iorValue;
                    }
                }
                else {
                    return lhs.colorsByChannel < rhs.colorsByChannel;
                }
            }
            else {
                return lhs.textureNameByChannel < rhs.textureNameByChannel;
            }
        }
    };

    /**
     * 3DF/HPS PBR material input texture channels.
     */
    enum PBRTextureChannels { Diffuse = 0, Roughness = 1, Metalness = 2, AmbientOcclusion = 3, Normal = 4 };

    /**
     * 3DF/HPS PBR input value channels.
     */
    enum PBRValueChannels { RoughnessFactor = 0, MetalnessFactor = 1, OcclusionFactor = 2, AlphaFactor = 3 };

    /**
     * 3DF/HPS PBR to Luminate realistic material conversion input.
     */
    struct PBRToRealisticMaterialInput {
        // Texture names associated to PBR channels indexed by the channel identifier.
        std::vector<std::string> textureNameByChannel = std::vector<std::string>(5, "");

        // Values associated to PBR channels indexed by the channel identifier.
        std::vector<float> valueByChannel = std::vector<float>(4, 0.f);

        // Individual parameters value.
        RED::Color diffuseColor = RED::Color::BLACK;
    };

    struct PBRToRealisticMaterialInputComparator {
        bool operator()(PBRToRealisticMaterialInput const& lhs, PBRToRealisticMaterialInput const& rhs) const
        {
            if (lhs.textureNameByChannel == rhs.textureNameByChannel) {
                if (lhs.valueByChannel == rhs.valueByChannel) {
                    return lhs.diffuseColor < rhs.diffuseColor;
                }
                else {
                    return lhs.valueByChannel < rhs.valueByChannel;
                }
            }
            else {
                return lhs.textureNameByChannel < rhs.textureNameByChannel;
            }
        }
    };

    /**
     * 3DF/HPS PBR to Luminate realistic material conversion output.
     */
    struct PBRToRealisticMaterialOutput {
        std::string diffuseTextureName;
        std::string reflectionTextureName;
        std::string anisotropyTextureName;
        std::string iorTextureName;
        std::string normalTextureName;
    };

    /**
     * Type of map storing for each 3DF/HPS PBR material definition its Luminate
     * realistic material conversion.
     */
    using PBRToRealisticConversionMap =
        std::map<PBRToRealisticMaterialInput, PBRToRealisticMaterialOutput, PBRToRealisticMaterialInputComparator>;

    /**
     * Type of map storing for each 3DF/HPS image name the created Luminate image object.
     */
    using ImageNameToLuminateMap = std::map<std::string, RED::Object*>;

    /**
     * Type of map storing for each 3DF/HPS texture name the source 3DF/HPS image name.
     */
    using TextureNameImageNameMap = std::map<std::string, std::string>;

    /**
     * Type of map storing created Luminate material instance according to a RealisticMaterialInfo.
     */
    using RealisticMaterialMap = std::map<RealisticMaterialInfo, RED::Object*, RealisticMaterialInfoComparator>;

    /**
     * Structure 3DF/HPS image data required to convert it to a Luminate image.
     */
    struct ImageData {
        std::string name;
        int width;
        int height;
        int bytesPerPixel;
        unsigned char* pixelData = nullptr;
    };

    /**
     * Structure gathering essential data describing the scene during the 3DF/HPS to Luminate conversion.
     * - Handedness of the view, required by views transformation between 3DF/HPS and Luminate like camera synchronisations.
     * - Luminate root shape containing the whole scene as children.
     * - Map associating 3DF/HPS image name and its instance of Luminate image.
     * - Map associating 3DF/HPS texture name and its source 3DF/HPS image name.
     * - Map associated 3DF/HPS PBR to Luminate realistic material conversion input with its conversion output.
     * - Map associating a material description and its Luminate material instance.
     * - Default material description from which all materials will be built, overriding some attributes.
     */
    struct LuminateSceneInfo {
        Handedness viewHandedness = Handedness::LeftHanded;
        RED::Object* rootTransformShape = nullptr;
        RED::Object* modelTransformShape = nullptr;
        RED::Object* floorMesh = nullptr;
        ImageNameToLuminateMap imageNameToLuminateMap;
        TextureNameImageNameMap textureNameImageNameMap;
        PBRToRealisticConversionMap pbrToRealisticConversionMap;
        RealisticMaterialMap materials;
        RealisticMaterialInfo defaultMaterialInfo;
    };

    using LuminateSceneInfoPtr = std::shared_ptr<LuminateSceneInfo>;

    /**
     * Creates a Luminate image from data coming from 3DF/HPS.
     * @param[in] a_imageData 3DF/HPS image data.
     * @param[in] a_resourceManager Luminate resource manager.
     * @return Luminate image or nullptr if failed.
     */
    RED::Object* createREDIImage2D(ImageData const& a_imageData, RED::Object* a_resourceManager);

    /**
     * Initialize the default material description.
     */
    RealisticMaterialInfo createDefaultRealisticMaterialInfo();

    /**
     * Creates a Luminate material from a material description.
     * @param[in] a_materialInfo Material description.
     * @param[in] a_resourceManager Luminate resource manager.
     * @param[in] a_imageNameToLuminateMap Mapping between 3DF/HPS image name and their Luminate image equivalent.
     * @param[in] a_textureNameImageNameMap Mapping between 3DF/HPS texture name and their 3DF/HPS source image name.
     * @param[inout] a_ioMaterialMap Mapping between a material description and already created Luminate materials.
     * @return Luminate material or nullptr if failed.
     */
    RED::Object* createREDMaterial(RealisticMaterialInfo const& a_materialInfo,
                                   RED::Object* a_resourceManager,
                                   ImageNameToLuminateMap const& a_imageNameToLuminateMap,
                                   TextureNameImageNameMap const& a_textureNameImageNameMap,
                                   RealisticMaterialMap& a_ioMaterialMap);

    /**
     * Creates a Luminate mesh shape from a polygon described by its points.
     * @param[in] a_state Current Luminate state.
     * @param[in] a_pointsLength Length of the points array.
     * @param[in] a_points Flat points array.
     * @return Luminate mesh shape.
     */
    RED::Object* createPolygonREDMeshShape(const RED::State& a_state, int a_pointslength, float* a_points);

    /**
     * Sets normals on a mesh shape.
     * @param[in] a_state Current Luminate state.
     * @param[in] a_dstShape Mesh shape where to set normals.
     * @param[in] a_nbVertices Normal count.
     * @param[in] a_normals Normals to set.
     * @return RED_OK if success, error code otherwise.
     */
    RED_RC setVertexNormals(const RED::State& a_state, RED::IMeshShape* a_dstShape, int a_nbVertices, float* a_normals);

    /**
     * Checks if a face defined by its 3 points indices in a point array, need to revert its widing according to their associated
     * normal.
     * @param[in] a_points Point array.
     * @param[in] a_normals Normal array.
     * @param[in] a_indiceA First triangle point index.
     * @param[in] a_indiceB Second triangle point index.
     * @param[in] a_indiceC Third triangle point index.
     * @return True if the face should be reversed, otherwise False.
     */
    bool needToReverseFaces(float* a_points, float* a_normals, int a_indiceA, int a_indiceB, int a_indiceC);

    /**
     * Creates a Luminate mesh shape from triangle strip data.
     * @param[in] a_state Current Luminate state
     * @param[in] a_tristrips_length Count of tristrip indices.
     * @param[in] a_tristrips Tristrip indices.
     * @param[in] a_pointsLength Count of points.
     * @param[in] a_points Tristrip points.
     * @param[in] a_normals Tristrip points normals.
     * @return Luminate mesh shape.
     */
    RED::Object* createREDMeshShapeFromTriStrips(const RED::State& a_state,
                                                 int a_tristrips_length,
                                                 int* a_tristrips,
                                                 int a_pointslength,
                                                 float* a_points,
                                                 float* a_normals);

    /**
     * Fills structure with 3DF/HPS PBR to Luminate realistic material conversion input data.
     * @param[in] a_baseColorFactor Base diffuse color.
     * @param[in] a_albedoTextureName Diffuse texture name.
     * @param[in] a_roughnessTextureName Roughness texture name.
     * @param[in] a_metalnessTextureName Metalness texture name.
     * @param[in] a_occlusionTextureName Occlusion texture name.
     * @param[in] a_normalTextureName Normal texture name.
     * @param[in] a_roughnessFactor Roughness factor.
     * @param[in] a_metalnessFactor Metalness factor.
     * @param[in] a_occlusionFactor Occlusion factor.
     * @param[in] a_alphaFactor Alpha factor.
     * @return Filled PBRToRealisticMaterialInput structure.
     */
    PBRToRealisticMaterialInput createPBRToRealisticMaterialInput(const RED::Color& a_baseColorFactor,
                                                                  std::string const& a_albedoTextureName,
                                                                  std::string const& a_roughnessTextureName,
                                                                  std::string const& a_metalnessTextureName,
                                                                  std::string const& a_occlusionTextureName,
                                                                  std::string const& a_normalTextureName,
                                                                  float a_roughnessFactor,
                                                                  float a_metalnessFactor,
                                                                  float a_occlusionFactor,
                                                                  float a_alphaFactor);

    /**
     * Lookup for an existing 3DF/HPS PBR to Luminate realistic material conversion and fills the associated material info if
     * found.
     * @param[in] a_conversionInput PBR to realistic material input.
     * @param[in] a_PBRToRealisticConversionMap Cache map of conversion results by input.
     * @param[inout] a_ioMaterialInfo RealisticMaterialInfo to fills if the conversion result is retrieved from the cache.
     * @return Whether or not the result has been retrieved from the cache.
     */
    bool retrievePBRToRealisticMaterialConversion(PBRToRealisticMaterialInput const& a_conversionInput,
                                                  PBRToRealisticConversionMap const& a_PBRToRealisticConversionMap,
                                                  RealisticMaterialInfo& a_ioMaterialInfo);

    /**
     * Converts a 3DF/HPS PBR material definition to Luminate realistic material.
     * @param[in] a_resourceManager Luminate resource manager.
     * @param[in] a_textureBaseName Unique base name used to set unique name for textures generated during conversion.
     * @param[in] a_albedoImage Luminate albedo texture.
     * @param[in] a_roughnessImage Luminate roughness texture.
     * @param[in] a_metalnessImage Luminate metalness texture.
     * @param[in] a_ambientOcclusionImage Luminate ambient occlusion texture.
     * @param[in] a_normalTextureName 3DF/HPS normal texture name.
     * @param[in] a_conversionInput PBR to realistic material input.
     * @param[in] a_ioImageNameToLuminateMap Cache of 3DF/HPS image name converted to Luminate texture.
     * @param[in] a_ioTextureNameImageNameMap Cache of 3DF/HPS texture name with associated 3DF/HPS image name.
     * @param[in] a_ioPBRToRealisticConversionMap Cache of PBR conversions associated to an input.
     * @param[in] a_ioMaterialInfo Resulting material info.
     */
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
                                       RealisticMaterialInfo& a_ioMaterialInfo);
} // namespace HC_luminate_bridge

#endif