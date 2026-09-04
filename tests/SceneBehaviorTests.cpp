#include "Scene.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <string>

namespace
{
    int failures = 0;

    void expect(bool condition, const std::string& message)
    {
        if (condition) return;

        ++failures;
        std::cerr << "FAILED: " << message << '\n';
    }

    bool nearlyEqual(float lhs, float rhs)
    {
        return std::fabs(lhs - rhs) < 0.0001f;
    }

    void objectLifecyclePreservesObjectState()
    {
        Scene scene;
        auto material = std::make_shared<Material>("baseline material");

        scene.AddObject(
            nullptr,
            glm::vec3(2.0f, -3.0f, 4.0f),
            glm::vec3(0.5f, 2.0f, 3.0f),
            material);

        expect(scene.GetObjects().size() == 1, "AddObject should append one object");

        const RenderObject& object = scene.GetObjects().front();
        expect(object.material == material, "AddObject should preserve the material association");

        const glm::mat4 model = object.transform.getModelMatrix();
        expect(nearlyEqual(model[3][0], 2.0f), "object translation X should be preserved");
        expect(nearlyEqual(model[3][1], -3.0f), "object translation Y should be preserved");
        expect(nearlyEqual(model[3][2], 4.0f), "object translation Z should be preserved");
        expect(nearlyEqual(model[0][0], 0.5f), "object scale X should be preserved");
        expect(nearlyEqual(model[1][1], 2.0f), "object scale Y should be preserved");
        expect(nearlyEqual(model[2][2], 3.0f), "object scale Z should be preserved");

        expect(!scene.RemoveObject(1), "removing an out-of-range object should fail");
        expect(scene.GetObjects().size() == 1, "failed removal should not change the scene");
        expect(scene.RemoveObject(0), "removing an existing object should succeed");
        expect(scene.GetObjects().empty(), "successful removal should erase the object");
    }

    void pointLightLimitIsEnforced()
    {
        Scene scene;
        const Light pointLight(
            glm::vec3(1.0f),
            1.0f,
            glm::vec3(0.0f),
            LightType::Point);

        for (std::size_t i = 0; i < RenderLimits::MaxPointLights; ++i)
        {
            expect(scene.AddPointLight(pointLight), "a point light within the limit should be accepted");
        }

        expect(!scene.AddPointLight(pointLight), "a point light beyond the limit should be rejected");
        expect(
            scene.GetPointLights().size() == RenderLimits::MaxPointLights,
            "rejected point lights should not change the scene");
    }

    void clearResetsSceneOwnedState()
    {
        Scene scene;
        scene.AddObject(nullptr);
        scene.AddPointLight(Light(
            glm::vec3(1.0f),
            1.0f,
            glm::vec3(0.0f),
            LightType::Point));

        auto environment = std::make_shared<EnvironmentAsset>();
        environment->hdrTexture = 42;
        scene.SetEnvironment(environment);

        scene.Clear();

        expect(scene.GetObjects().empty(), "Clear should remove all objects");
        expect(scene.GetPointLights().empty(), "Clear should remove all point lights");
        expect(!scene.GetEnvironment().asset, "Clear should release the selected environment");
    }
}

int main()
{
    objectLifecyclePreservesObjectState();
    pointLightLimitIsEnforced();
    clearResetsSceneOwnedState();

    if (failures != 0)
    {
        std::cerr << failures << " scene behavior test(s) failed.\n";
        return 1;
    }

    std::cout << "All scene behavior tests passed.\n";
    return 0;
}
