#pragma once
#include <glm/glm.hpp>

enum class AmbientLightingMode { None, Hemisphere, IBL };

struct AmbientLighting
{
    AmbientLightingMode mode = AmbientLightingMode::IBL;
    float hemisphereIntensity = 0.2f;
    float iblIntensity = 1.0f;
    // Linear colors; the hemisphere follows world +Y, independent of the camera.
    glm::vec3 skyColor{0.65f, 0.75f, 1.0f};
    glm::vec3 groundColor{0.2f, 0.18f, 0.15f};
};
