#include "Scene.h"
#include "RenderSettings.h"
#include "RenderTypes.h"

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
        expect(
            object.material.baseMaterial == material,
            "AddObject should preserve the material association");

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

    void materialInstancesFollowTheirObjects()
    {
        Scene scene;
        auto firstMaterial = std::make_shared<Material>("first material");
        auto secondMaterial = std::make_shared<Material>("second material");

        scene.AddObject(nullptr, glm::vec3(1.0f), glm::vec3(1.0f), firstMaterial);
        scene.AddObject(nullptr, glm::vec3(2.0f), glm::vec3(1.0f), secondMaterial);

        scene.GetObjects()[0].material.aoBias = -0.25f;
        scene.GetObjects()[0].material.useNormalMap = false;
        scene.GetObjects()[1].material.aoBias = 0.75f;
        scene.GetObjects()[1].material.roughnessBias = 0.5f;
        scene.GetObjects()[1].material.metallicBias = -0.5f;
        scene.GetObjects()[1].material.useNormalMap = true;

        expect(scene.RemoveObject(0), "the first object should be removable");
        expect(scene.GetObjects().size() == 1, "one object should remain after removal");

        const MaterialInstance& remaining = scene.GetObjects().front().material;
        expect(
            remaining.baseMaterial == secondMaterial,
            "material ownership should follow the surviving object");
        expect(nearlyEqual(remaining.aoBias, 0.75f), "AO bias should follow the surviving object");
        expect(
            nearlyEqual(remaining.roughnessBias, 0.5f),
            "roughness bias should follow the surviving object");
        expect(
            nearlyEqual(remaining.metallicBias, -0.5f),
            "metallic bias should follow the surviving object");
        expect(remaining.useNormalMap, "normal-map preference should follow the surviving object");

        scene.AddObject(nullptr);
        const MaterialInstance& defaults = scene.GetObjects().back().material;
        expect(!defaults.baseMaterial, "a new object should not invent a base material");
        expect(nearlyEqual(defaults.aoBias, 0.0f), "a new object should start with zero AO bias");
        expect(
            nearlyEqual(defaults.roughnessBias, 0.0f),
            "a new object should start with zero roughness bias");
        expect(
            nearlyEqual(defaults.metallicBias, 0.0f),
            "a new object should start with zero metallic bias");
        expect(!defaults.useNormalMap, "normal mapping should preserve its previous default-off behavior");
    }

    void renderStateDefaultsPreserveCurrentBehavior()
    {
        const RenderSettings settings;
        expect(!settings.deferred, "forward rendering should remain the default path");
        expect(!settings.shadows, "shadows should remain disabled by default");
        expect(!settings.groundPlane.visible, "the ground plane should remain hidden by default");
        expect(!settings.postProcess.enabled, "post processing should remain disabled by default");
        expect(settings.postProcess.hdr, "HDR should remain enabled when post processing is used");
        expect(!settings.postProcess.bloom, "bloom should remain disabled by default");
        expect(nearlyEqual(settings.postProcess.exposure, 1.0f), "default exposure should remain 1.0");

        RenderExtent extent;
        expect(!extent.isValid(), "a default render extent should be invalid");
        extent.width = 1920;
        extent.height = 1080;
        expect(extent.isValid(), "a non-zero render extent should be valid");

        RenderOutput output;
        expect(output.colorTexture == 0, "a default render output should not expose a texture");
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

        auto environment = std::make_shared<EnvironmentAsset>("environment.hdr");
        expect(environment->getPath() == "environment.hdr", "environment stores an asset path");
        expect(environment->getRevision() == 1, "environment starts at revision one");
        Scene otherScene;
        otherScene.SetEnvironment(environment);
        scene.SetEnvironment(environment);

        scene.Clear();

        expect(scene.GetObjects().empty(), "Clear should remove all objects");
        expect(scene.GetPointLights().empty(), "Clear should remove all point lights");
        expect(!scene.GetEnvironment().asset, "Clear should release the selected environment");
        expect(otherScene.GetEnvironment().asset == environment, "clearing one scene preserves another selection");
    }
}

int main()
{
    objectLifecyclePreservesObjectState();
    pointLightLimitIsEnforced();
    materialInstancesFollowTheirObjects();
    renderStateDefaultsPreserveCurrentBehavior();
    clearResetsSceneOwnedState();

    if (failures != 0)
    {
        std::cerr << failures << " scene behavior test(s) failed.\n";
        return 1;
    }

    std::cout << "All scene behavior tests passed.\n";
    return 0;
}
