#pragma once
#include <glm/glm.hpp>

struct CameraData
{
    glm::mat4 view{1.0f};
    glm::mat4 projection{1.0f};
    glm::vec3 position{0.0f};
    float fovDegrees = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

struct RenderFrameData
{
    float timeSeconds = 0.0f;
    bool directionalLightEnabled = false;
    bool pointLightsEnabled = false;
};
