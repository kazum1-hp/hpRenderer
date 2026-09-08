#pragma once
#include "hpr/assets/EnvironmentAsset.h"
#include "hpr/assets/SkyboxAsset.h"
#include "hpr/scene/Material.h"
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <vector>

class Model;
class GpuModel;

template<class ModelType> struct BasicRenderItem
{
    // Geometry stays shared; per-object render state is copied for this submission.
    std::shared_ptr<const ModelType> model;
    glm::mat4 transform{1.0f};
    MaterialInstance material;
};
using RenderItem = BasicRenderItem<Model>;
using GpuRenderItem = BasicRenderItem<GpuModel>;

struct DirectionalLightData
{
    glm::vec3 color{1.0f};
    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float intensity = 1.0f;
    bool enabled = true;
    glm::mat4 lightSpaceMatrix{1.0f};
};

struct PointLightData
{
    glm::vec3 color{1.0f};
    glm::vec3 position{0.0f};
    float intensity = 1.0f;
    float farPlane = 30.0f;
    bool enabled = true;
    std::array<glm::mat4, 6> shadowMatrices{
        glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f),
        glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};
};

template<class ModelType> struct BasicRenderScene
{
    // Per-submission value snapshot. Geometry/material assets and environment metadata
    // remain shared resources, not deep copies or thread-safe hot-reload snapshots.
    std::vector<BasicRenderItem<ModelType>> objects;
    DirectionalLightData directionalLight;
    std::vector<PointLightData> pointLights;
    std::shared_ptr<const EnvironmentAsset> environment;
    std::shared_ptr<const SkyboxAsset> skybox;
    EnvironmentMode environmentMode = EnvironmentMode::IBL;
};
using RenderScene = BasicRenderScene<Model>;
using GpuRenderScene = BasicRenderScene<GpuModel>;
