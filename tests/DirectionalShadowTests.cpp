#include "hpr/renderer/DirectionalShadow.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
void require(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
void inside(const glm::mat4& matrix, glm::vec3 point)
{
    const glm::vec4 clip = matrix * glm::vec4(point, 1);
    require(glm::all(glm::lessThanEqual(glm::abs(glm::vec3(clip) / clip.w), glm::vec3(1.0001f))),
        "receiver/caster clipped from shadow coverage");
}
void sameMatrix(const glm::mat4& a, const glm::mat4& b)
{
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            require(std::isfinite(a[i][j]) && std::abs(a[i][j] - b[i][j]) < 0.0001f,
                "unstable or invalid shadow projection");
}
CameraData cameraAt(glm::vec3 position, float aspect)
{
    CameraData camera;
    camera.position = position;
    camera.nearPlane = 0.1f;
    camera.farPlane = 1000;
    camera.view = glm::lookAt(position, position + glm::vec3(0, 0, -1), glm::vec3(0, 1, 0));
    camera.projection = glm::perspective(glm::radians(60.0f), aspect, camera.nearPlane, camera.farPlane);
    return camera;
}
}
int main()
{
    try
    {
        for (float aspect : {1.0f, 16.0f / 9.0f})
            for (glm::vec3 position : {glm::vec3(0), glm::vec3(320, 40, -200)})
                for (glm::vec3 direction : {glm::vec3(-2.2f, -2, -2.3f), glm::vec3(0, -1, 0), glm::vec3(0)})
                {
                    const auto camera = cameraAt(position, aspect);
                    const auto shadow = BuildDirectionalShadowProjection(camera, direction, 100, 1024, {});
                    for (float depth : {0.1f, 100.0f})
                        for (float x : {-1.0f, 1.0f})
                            for (float y : {-1.0f, 1.0f})
                                inside(shadow.matrix, position + glm::vec3(x * depth * std::tan(glm::radians(30.0f)) * aspect,
                                    y * depth * std::tan(glm::radians(30.0f)), -depth));
                    sameMatrix(shadow.matrix, BuildDirectionalShadowProjection(camera, direction * 10.0f, 100, 1024, {}).matrix);
                }
        const auto camera = cameraAt(glm::vec3(0), 1);
        const Bounds upstream{glm::vec3(-1, 499, -21), glm::vec3(1, 501, -19)};
        const auto shadow = BuildDirectionalShadowProjection(camera, {0, -1, 0}, 100, 1024, {upstream});
        for (int i = 0; i < 8; ++i) inside(shadow.matrix, upstream.corner(i));
        inside(shadow.matrix, {0, 0, -20});
        const auto empty = BuildDirectionalShadowProjection(camera, {0, -1, 0}, 100, 1024, {});
        require(shadow.depthRange > empty.depthRange, "off-camera caster did not extend depth range");
        const Bounds unrelated{glm::vec3(10000, 500, 0), glm::vec3(10002, 502, 2)};
        sameMatrix(empty.matrix, BuildDirectionalShadowProjection(camera, {0, -1, 0}, 100, 1024, {unrelated}).matrix);
        const auto tinyMove = cameraAt({0.001f, 0, 0}, 1);
        sameMatrix(empty.matrix, BuildDirectionalShadowProjection(tinyMove, {0, -1, 0}, 100, 1024, {}).matrix);
        const auto shortRange = BuildDirectionalShadowProjection(camera, {0, -1, 0}, 20, 1024, {});
        require(std::abs(shortRange.matrix[0][0]) > std::abs(empty.matrix[0][0]), "shadow distance did not affect coverage");
        const Bounds local{glm::vec3(-1), glm::vec3(1)};
        const auto transform = glm::translate(glm::mat4(1), glm::vec3(30, 50, -20)) *
            glm::rotate(glm::mat4(1), 0.7f, glm::vec3(0, 1, 0)) * glm::scale(glm::mat4(1), glm::vec3(-2, 3, 4));
        const auto world = local.transformed(transform);
        const auto transformedShadow = BuildDirectionalShadowProjection(camera, {0, -1, 0}, 100, 1024, {world});
        for (int i = 0; i < 8; ++i) inside(transformedShadow.matrix, world.corner(i));
        std::cout << "Directional shadow coverage tests passed.\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
