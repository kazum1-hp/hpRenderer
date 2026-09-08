#pragma once
#include "hpr/assets/TextureTypes.h"
#include <glm/glm.hpp>
#include <cstddef>
#include <string>
#include <vector>

// Owned CPU data: no Assimp pointers, OpenGL objects or editor dependencies.
enum class AlphaMode
{
    Opaque,
    Mask,
    Blend
};
struct ModelImage
{
    std::vector<unsigned char> bytes;
    unsigned int width = 0, height = 0; // Both zero: encoded PNG/JPEG/etc.; otherwise tightly packed RGBA8.
};
struct ModelTexture
{
    TextureType semantic = Diffuse;
    std::string path;
    int embeddedImage = -1;
    int channel = 0;
};
struct ModelMaterial
{
    std::string name;
    glm::vec4 baseColor{1.0f};
    float roughness = 0.5f, metallic = 0.0f;
    AlphaMode alphaMode = AlphaMode::Opaque;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;
    bool inferAlphaFromTextures = false; // Legacy formats without an explicit alpha mode.
    std::vector<ModelTexture> textures;
};
struct ModelMeshData
{
    std::vector<float> vertices; // Position, normal, UV0, tangent, bitangent (14 floats).
    std::vector<unsigned int> indices;
    std::size_t materialIndex = 0;
    glm::vec3 center{0.0f};
    bool hasTangents = false;
};
struct ModelNode
{
    std::string name;
    int parent = -1;
    glm::mat4 localTransform{1.0f};
    glm::mat4 worldTransform{1.0f};
};
struct ModelDraw
{
    std::size_t meshIndex = 0, nodeIndex = 0;
};
struct ModelAsset
{
    std::string path;
    std::vector<ModelImage> images;
    std::vector<ModelMaterial> materials;
    std::vector<ModelMeshData> meshes;
    std::vector<ModelNode> nodes;
    std::vector<ModelDraw> draws;
    std::vector<std::string> warnings;
};
