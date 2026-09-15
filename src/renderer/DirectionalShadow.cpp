#include "hpr/renderer/DirectionalShadow.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <array>

glm::vec3 NormalizeLightDirection(glm::vec3 direction)
{
    const float length = glm::length(direction);
    return std::isfinite(length) && length > 0.00001f ? direction / length : glm::vec3(0, -1, 0);
}

DirectionalShadowProjection BuildDirectionalShadowProjection(const CameraData& camera,
    glm::vec3 direction, float shadowDistance, unsigned int resolution,
    const std::vector<Bounds>& worldCasterBounds)
{
    direction = NormalizeLightDirection(direction);
    const glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
    // Rotation only: keeps the texel grid anchored in world space.
    const glm::mat4 lightView = glm::lookAt(glm::vec3(0), direction, up);
    const glm::mat4 inverseCamera = glm::inverse(camera.projection * camera.view);
    const float distance = std::clamp(shadowDistance, camera.nearPlane + 0.001f, camera.farPlane);
    const float fraction = (distance - camera.nearPlane) / (camera.farPlane - camera.nearPlane);
    std::array<glm::vec3, 8> corners;
    glm::vec3 center(0);
    for (int i = 0; i < 4; ++i)
    {
        const float x = i & 1 ? 1.0f : -1.0f;
        const float y = i & 2 ? 1.0f : -1.0f;
        glm::vec4 nearPoint = inverseCamera * glm::vec4(x, y, -1, 1);
        glm::vec4 farPoint = inverseCamera * glm::vec4(x, y, 1, 1);
        corners[i] = glm::vec3(nearPoint) / nearPoint.w;
        corners[i + 4] = glm::mix(corners[i], glm::vec3(farPoint) / farPoint.w, fraction);
        center += corners[i] + corners[i + 4];
    }
    center /= 8.0f;
    float radius = 0.0f;
    Bounds receivers;
    for (const auto& corner : corners)
    {
        radius = std::max(radius, glm::length(corner - center));
        receivers.include(glm::vec3(lightView * glm::vec4(corner, 1)));
    }
    // A sphere preserves XY coverage when the camera rotates. Quantization and
    // texel snapping prevent tiny camera movements from sliding the shadow grid.
    radius = std::max(0.0625f, std::ceil(radius * 16.0f) / 16.0f);
    const float size = static_cast<float>(std::max(resolution, 8u));
    radius *= size / (size - 4.0f); // PCF and snapping guard band.
    const float texel = 2.0f * radius / size;
    glm::vec3 lightCenter(lightView * glm::vec4(center, 1));
    lightCenter.x = std::round(lightCenter.x / texel) * texel;
    lightCenter.y = std::round(lightCenter.y / texel) * texel;
    const glm::vec2 lower = glm::vec2(lightCenter) - radius;
    const glm::vec2 upper = glm::vec2(lightCenter) + radius;
    float minZ = receivers.min.z, maxZ = receivers.max.z;
    for (const auto& bounds : worldCasterBounds)
    {
        const Bounds caster = bounds.transformed(lightView);
        if (!caster.valid() || caster.max.x < lower.x || caster.min.x > upper.x ||
            caster.max.y < lower.y || caster.min.y > upper.y || caster.max.z < receivers.min.z)
            continue;
        // +Z faces the sun. Include off-camera upstream occluders.
        maxZ = std::max(maxZ, caster.max.z);
    }
    const float padding = std::max(1.0f, (maxZ - minZ) * 0.01f);
    minZ -= padding;
    maxZ += padding;
    return {glm::ortho(lower.x, upper.x, lower.y, upper.y, -maxZ, -minZ) * lightView, maxZ - minZ};
}
