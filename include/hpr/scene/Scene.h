#pragma once
#include <vector>
#include <memory>
#include <cstddef>
#include "hpr/assets/Model.h"
#include "hpr/scene/Light.h"
#include "hpr/scene/AmbientLighting.h"
#include "hpr/scene/Transform.h"
#include "hpr/scene/Material.h"
#include "hpr/renderer/RenderLimits.h"
#include "hpr/assets/EnvironmentAsset.h"
#include "hpr/assets/SkyboxAsset.h"

// In order to place the model in the scene, we need a struct to store the model pointer and position information.
struct RenderObject {
    std::shared_ptr<Model> model;
    Transform transform;
    MaterialInstance material;
};

struct Environment
{
    std::shared_ptr<const EnvironmentAsset> asset;
    std::shared_ptr<const SkyboxAsset> skybox;
    EnvironmentMode mode = EnvironmentMode::IBL;
    AmbientLighting lighting;
};

class Scene {
public:
    // A default directional light
    Scene() :dirLight(glm::vec3(1.0f), 1.0f, glm::vec3(-2.2f, -2.0f, -2.3f), LightType::Directional) {}

    // --- Add Objects ---
    void AddObject(std::shared_ptr<Model> model, glm::vec3 pos = glm::vec3(0.0f), glm::vec3 scale = glm::vec3(1.0f), MaterialInstance material = {}) {
        RenderObject obj;
        obj.model = model;
        obj.transform.setInitialTransform(pos, scale);
        obj.material = std::move(material);
        objects.push_back(obj);
    }

    bool RemoveObject(std::size_t index) {
        if (index >= objects.size()) return false;
        objects.erase(objects.begin() + index);
        return true;
    }

    // --- Lighting Management ---
    bool AddPointLight(const Light& light) {
        if (pointLights.size() >= RenderLimits::MaxPointLights) return false;
        pointLights.push_back(light);
        return true;
    }

    Light& GetPointLight(std::size_t index) { return pointLights.at(index); }

    void Clear() {
        objects.clear();
        pointLights.clear();
        environment = {};
    }

    void SetEnvironment(std::shared_ptr<const EnvironmentAsset> envAsset) {
        environment.asset = envAsset;
    }

    // --- Getters ---
    const std::vector<RenderObject>& GetObjects() const { return objects; }
    std::vector<RenderObject>& GetObjects() { return objects; }
    const std::vector<Light>& GetPointLights() const { return pointLights; }
    const Light& GetDirLight() const { return dirLight; }
    Light& GetDirLight() { return dirLight; }

    
    Environment& GetEnvironment() { return environment; }
    const Environment& GetEnvironment() const { return environment; }

private:
    std::vector<RenderObject> objects;
    std::vector<Light> pointLights;
    Light dirLight;
    Environment environment;
};
