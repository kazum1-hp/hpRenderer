#pragma once
#include "hpr/assets/Bounds.h"
#include "hpr/renderer/RenderFrameData.h"
#include <vector>

struct DirectionalShadowProjection
{
    glm::mat4 matrix{1.0f};
    float depthRange = 1.0f;
};

glm::vec3 NormalizeLightDirection(glm::vec3 direction);

// Receiver coverage follows the camera. Casters only extend the light-space depth,
// so distant objects cannot waste the shadow map's horizontal resolution.
DirectionalShadowProjection BuildDirectionalShadowProjection(const CameraData& camera,
    glm::vec3 direction, float shadowDistance, unsigned int resolution,
    const std::vector<Bounds>& worldCasterBounds);
