#pragma once
#include <vector>
#include <memory>
#include "Model.h"
#include "Light.h"
#include "Skybox.h"
#include "Transform.h"
#include "Material.h"

// In order to place the model in the scene, we need a struct to store the model pointer and position information.
struct RenderObject {
    std::shared_ptr<Model> model;
    Transform transform;
    // If need to override the model's default material, add the Material here.
    std::shared_ptr<Material> material;
};

struct EnvironmentAsset {
    GLuint hdrTexture;
};

struct EnvironmentMaps {
    GLuint envCubemap = 0;
    GLuint irradianceMap = 0;
    GLuint prefilterMap = 0;
    GLuint brdfLUT = 0;

    bool isGenerated = false;
    GLuint lastHDR = 0; // Prevent duplicate generation
};

struct Environment {
    std::shared_ptr<EnvironmentAsset> asset;
    EnvironmentMaps maps;
};

//struct EnvironmentData {
//    GLuint hdrTexture;        // origin HDR texture
//    GLuint envCubemap;       // Converted cube environment map
//    GLuint irradianceMap;    // irradianceMap
//    GLuint prefilterMap;     // prefilterMap
//    GLuint brdfLUT;          // BRDF Lookup table
//};

class Scene {
public:
    // A default directional light
    Scene() :dirLight(glm::vec3(1.0f), 1.0f, glm::vec3(-2.2f, -2.0f, -2.3f), LightType::Directional) {}

    // --- Add Objects ---
    void AddObject(std::shared_ptr<Model> model, glm::vec3 pos = glm::vec3(0.0f), glm::vec3 scale = glm::vec3(1.0f), std::shared_ptr<Material> mat = nullptr) {
        RenderObject obj;
        obj.model = model;
        obj.transform.setPosition(pos);
        obj.transform.setScale(scale);
        obj.material = mat;
        objects.push_back(obj);
    }

    // --- Lighting Management ---
    void AddPointLight(const Light& light) {
        pointLights.push_back(light);
    }

    void SetEnvironment(std::shared_ptr<EnvironmentAsset> envAsset) {
        environment.asset = envAsset;
        environment.maps.isGenerated = false; // The tag needs to be regenerated.
    }

    // --- Getters ---
    const std::vector<RenderObject>& GetObjects() const { return objects; }
    std::vector<RenderObject>& GetObjects() { return objects; }
    const std::vector<Light>& GetPointLights() const { return pointLights; }
    std::vector<Light>& GetPointLights() { return pointLights; }
    const Light& GetDirLight() const { return dirLight; }
    Light& GetDirLight() { return dirLight; }

    //void SetSkybox(std::shared_ptr<Skybox> sb) { skybox = sb; }
    //const Skybox* GetSkybox() const { return skybox.get(); }
    
    Environment& GetEnvironment() { return environment; }
    const Environment& GetEnvironment() const { return environment; }
    //const GLuint GetSkybox() const { return env.hdrTexture; }

private:
    std::vector<RenderObject> objects;
    std::vector<Light> pointLights;
    Light dirLight;
    std::shared_ptr<Skybox> skybox;
    Environment environment;
};
