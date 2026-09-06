#include "Renderer.h"
#include "RenderExtraction.h"

#if defined(_glfw3_h_) || defined(IMGUI_VERSION)
#error Renderer public API must not depend on GLFW or ImGui
#endif

#include "Scene.h"
#include "Camera.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }
}

int main()
{
    try
    {
        RenderScene snapshot;
        {
            Scene source;
            source.AddObject(nullptr, glm::vec3(2, 3, 4), glm::vec3(2));
            source.GetObjects()[0].material.roughnessBias = 0.7f;
            source.GetObjects()[0].material.useNormalMap = true;
            source.GetDirLight().setColor(glm::vec3(0.5f));
            source.GetDirLight().setEnabled(false);
            source.AddPointLight(Light(glm::vec3(0.3f), 4.0f, glm::vec3(1, 2, 3), LightType::Point));
            source.SetEnvironment(std::make_shared<EnvironmentAsset>("snapshot.hdr"));
            snapshot = BuildRenderScene(source);
            source.GetObjects()[0].material.roughnessBias = -0.2f;
            source.GetObjects()[0].transform.setPosition(glm::vec3(0));
            source.GetPointLight(0).setIntensity(9.0f);
            source.Clear();
        }
        require(snapshot.objects.size() == 1, "snapshot lost object after scene destruction");
        require(snapshot.objects[0].transform[3] == glm::vec4(2, 3, 4, 1), "transform was not copied");
        require(snapshot.objects[0].material.roughnessBias == 0.7f &&
            snapshot.objects[0].material.useNormalMap, "material overrides were not copied");
        require(snapshot.pointLights.size() == 1 && snapshot.pointLights[0].intensity == 4.0f &&
            snapshot.pointLights[0].position == glm::vec3(1, 2, 3), "point light was not copied");
        require(!snapshot.directionalLight.enabled && snapshot.directionalLight.color == glm::vec3(0.5f),
            "directional light was not copied");
        require(snapshot.environment->getPath() == "snapshot.hdr", "snapshot lost environment selection");
        Scene empty;
        require(BuildRenderScene(empty).objects.empty() && !BuildRenderScene(empty).environment,
            "empty scene inherited snapshot data");

        Camera camera;
        const auto originalPosition = camera.getPosition();
        const auto originalAspect = camera.aspect;
        const auto wide = BuildCameraData(camera, {160, 90});
        const auto square = BuildCameraData(camera, {90, 90});
        require(std::fabs(wide.projection[0][0] * (160.0f / 90.0f) - square.projection[0][0]) < 0.0001f,
            "projection must use each target's aspect ratio");
        require(camera.aspect == originalAspect && camera.getPosition() == originalPosition,
            "camera extraction mutated the source");
        bool invalid = false;
        try { BuildCameraData(camera, {0, 90}); } catch (const std::invalid_argument&) { invalid = true; }
        require(invalid, "zero camera extent must be rejected");

        // This process never initializes GLFW/GLAD and never creates a GL context.
        Renderer renderer;
        bool rejected = false;
        try { renderer.render({}, {}, {}, {}); } catch (const std::logic_error&) { rejected = true; }
        require(rejected, "render before initialization must be rejected without GL calls");
        renderer.shutdown();
        renderer.shutdown();
        std::cout << "Render submission CPU tests passed.\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
