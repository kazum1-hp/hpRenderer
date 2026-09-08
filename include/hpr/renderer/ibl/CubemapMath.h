#pragma once
#include <array>
#include <glm/gtc/matrix_transform.hpp>

inline std::array<glm::mat4, 6> CalculateCubemapMatrices(glm::vec3 center, float nearZ = 0.1f, float farZ = 100.0f)
{
    // The aspect of IBL and Shadow is always 1.0f.
    glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearZ, farZ);

    return {projection * glm::lookAt(center, center + glm::vec3(1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)),
            projection * glm::lookAt(center, center + glm::vec3(-1.0, 0.0, 0.0), glm::vec3(0.0, -1.0, 0.0)),
            projection * glm::lookAt(center, center + glm::vec3(0.0, 1.0, 0.0), glm::vec3(0.0, 0.0, 1.0)),
            projection * glm::lookAt(center, center + glm::vec3(0.0, -1.0, 0.0), glm::vec3(0.0, 0.0, -1.0)),
            projection * glm::lookAt(center, center + glm::vec3(0.0, 0.0, 1.0), glm::vec3(0.0, -1.0, 0.0)),
            projection * glm::lookAt(center, center + glm::vec3(0.0, 0.0, -1.0), glm::vec3(0.0, -1.0, 0.0))};
}
