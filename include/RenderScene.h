#pragma once
#include "EnvironmentAsset.h"
#include "Material.h"
#include <glm/glm.hpp>
#include <array>
#include <memory>
#include <vector>

class Model;

struct RenderItem
{
    // Geometry stays shared; per-object render state is copied for this submission.
    std::shared_ptr<const Model> model;
    glm::mat4 transform{1.0f};
    MaterialInstance material;
};

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

struct RenderScene
{
    // Per-submission value snapshot. Geometry/material assets and environment metadata
    // remain shared resources, not deep copies or thread-safe hot-reload snapshots.
    std::vector<RenderItem> objects;
    DirectionalLightData directionalLight;
    std::vector<PointLightData> pointLights;
    std::shared_ptr<const EnvironmentAsset> environment;
};
