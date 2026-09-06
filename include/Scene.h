#pragma once
#include <vector>
#include <memory>
#include <cstddef>
#include "Model.h"
#include "Light.h"
#include "Skybox.h"
#include "Transform.h"
#include "Material.h"
#include "RenderLimits.h"
#include "EnvironmentAsset.h"

// In order to place the model in the scene, we need a struct to store the model pointer and position information.
struct RenderObject {
    std::shared_ptr<Model> model;
    Transform transform;
    MaterialInstance material;
};

struct Environment
{
    std::shared_ptr<const EnvironmentAsset> asset;
};

class Scene {
public:
    // A default directional light
    Scene() :dirLight(glm::vec3(1.0f), 1.0f, glm::vec3(-2.2f, -2.0f, -2.3f), LightType::Directional) {}

    // --- Add Objects ---
    void AddObject(std::shared_ptr<Model> model, glm::vec3 pos = glm::vec3(0.0f), glm::vec3 scale = glm::vec3(1.0f), std::shared_ptr<Material> mat = nullptr) {
        RenderObject obj;
        obj.model = model;
        obj.transform.setInitialTransform(pos, scale);
        obj.material.baseMaterial = mat;
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
        skybox.reset();
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

    //void SetSkybox(std::shared_ptr<Skybox> sb) { skybox = sb; }
    //const Skybox* GetSkybox() const { return skybox.get(); }
    
    Environment& GetEnvironment() { return environment; }
    const Environment& GetEnvironment() const { return environment; }

private:
    std::vector<RenderObject> objects;
    std::vector<Light> pointLights;
    Light dirLight;
    std::shared_ptr<Skybox> skybox;
    Environment environment;
};
