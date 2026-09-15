#pragma once
#include <glm/glm.hpp>
#include <limits>

struct Bounds
{
    glm::vec3 min{std::numeric_limits<float>::max()};
    glm::vec3 max{std::numeric_limits<float>::lowest()};

    bool valid() const { return glm::all(glm::lessThanEqual(min, max)); }
    void include(glm::vec3 point)
    {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }
    glm::vec3 corner(int i) const
    {
        return {i & 1 ? max.x : min.x, i & 2 ? max.y : min.y, i & 4 ? max.z : min.z};
    }
    Bounds transformed(const glm::mat4& matrix) const
    {
        Bounds result;
        if (valid())
            for (int i = 0; i < 8; ++i)
                result.include(glm::vec3(matrix * glm::vec4(corner(i), 1.0f)));
        return result;
    }
};
