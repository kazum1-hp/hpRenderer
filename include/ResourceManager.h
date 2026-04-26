#pragma once
#include <glad/glad.h>
#include <unordered_map>
#include <string>
#include <memory>
#include <iostream>
#include <array>
#include <vector>

#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "glm/gtc/type_ptr.hpp"
#include "GLFW/glfw3.h"

#include "Shader.h"
#include "Texture.h"
#include "Model.h"
#include "Mesh.h"
#include "Material.h"

class ResourceManager {
public:
    static ResourceManager& GetInstance() {
        static ResourceManager instance;
        return instance;
    }

    // Copying is prohibited.
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;

    // --- Shader ---
    std::shared_ptr<Shader> LoadShader(const std::string& name, const std::string& vsPath, const std::string& fsPath, const std::string& gsPath = "") {
        if (shaders.find(name) != shaders.end()) return shaders[name];

        auto shader = std::make_shared<Shader>(vsPath, fsPath, gsPath);
        shaders[name] = shader;
        return shader;
    }

    std::shared_ptr<Shader> GetShader(const std::string& name) {
        if (shaders.find(name) != shaders.end()) return shaders[name];
        std::cerr << "Shader not found: " << name << std::endl;
        return nullptr;
    }

    // Reload a single shader by name (called in a thread with a GL context)
    bool ReloadShader(const std::string& name) {
        auto it = shaders.find(name);
        if (it == shaders.end()) {
            std::cerr << "ReloadShader: not found: " << name << std::endl;
            return false;
        }
        if (!it->second) return false;
        return it->second->reload();
    }

    // Reloads all loaded shaders and returns the reload result for each shader.
    std::vector<std::pair<std::string, bool>> ReloadAllShaders() {
        std::vector<std::pair<std::string, bool>> results;
        results.reserve(shaders.size());
        for (auto& kv : shaders) {
            bool ok = false;
            if (kv.second) {
                ok = kv.second->reload();
            }
            results.emplace_back(kv.first, ok);
        }
        return results;
    }

    // --- Texture ---
    std::shared_ptr<Texture> LoadTexture(const std::string& path, TextureType type = Diffuse) {
        if (textures.find(path) != textures.end()) return textures[path];

        auto tex = std::make_shared<Texture>(path.c_str(), type);
        textures[path] = tex;
        return tex;
    }

    // Replace/reload texture (returns a new shared_ptr)
    std::shared_ptr<Texture> ReloadTexture(const std::string& path, TextureType type = Diffuse) {
        auto tex = std::make_shared<Texture>(path, type);
        textures[path] = tex;
        return tex;
    }

    // --- Model ---
    std::shared_ptr<Model> LoadModel(const std::string& path) {
        if (models.find(path) != models.end()) return models[path];

        auto model = std::make_shared<Model>(path);
        models[path] = model;
        return model;
    }

    // Reload on an existing Model instance by path (if already loaded, call model->reload; if not loaded, call LoadModel).
    bool ReloadModel(const std::string& path) {
        auto it = models.find(path);
        if (it == models.end()) {
            // Loading not yet, load directly and return whether it was successful.
            try {
                auto model = std::make_shared<Model>(path);
                models[path] = model;
                return true;
            } catch (...) {
                return false;
            }
        } else {
            if (!it->second) return false;
            // Call Model::reload (this method needs to be provided by the Model).
            it->second->reload(path);
            return true;
        }
    }

    // --- Material ---
    std::shared_ptr<Material> LoadMaterial(const std::string& name) {
        if (materials.find(name) != materials.end()) return materials[name];

        auto material = std::make_shared<Material>(name);
        materials[name] = material;
        return material;
    }

    std::shared_ptr<Material> GetMaterial(const std::string& name) {
        if (materials.find(name) != materials.end()) return materials[name];
        return nullptr;
    }

    // --- Basic shapes (Quad, Cube) are used internally by the renderer. ---
    // It is recommended to implement a method to generate a full-screen Quad, because the Renderer needs it for post-processing.
    std::shared_ptr<Mesh> GetScreenQuad() {
        if (screenQuad) return screenQuad;

        std::vector<float> quadVertices = { // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
            // positions   // texCoords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };

        std::vector<unsigned int> indices = {
            0, 1, 2,
            0, 2, 3
        };

        std::vector<VertexAttribute> attributes = {
            { 0, 2, GL_FLOAT, GL_FALSE },
            { 1, 2, GL_FLOAT, GL_FALSE }
        };

        std::shared_ptr<Geometry> screen = std::make_shared<Geometry>(quadVertices, indices, attributes);
        screenQuad = std::make_shared<Mesh>(*screen);
        return screenQuad;
    }

    std::shared_ptr<Mesh> GetPlane()
    {
        if (plane) return plane;
 
        std::vector<float> planeVertices = {
            // positions            // normals         // texcoords
             25.0f, -1.0f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
            -25.0f, -1.0f,  25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
            -25.0f, -1.0f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,
             25.0f, -1.0f, -25.0f,  0.0f, 1.0f, 0.0f,  25.0f, 25.0f
        };

        std::vector<unsigned int> p_indices = {
            0, 1, 2,
            0, 2, 3
        };

        std::vector<VertexAttribute> f_attributes = {
            { 0, 3, GL_FLOAT, GL_FALSE },
            { 1, 3, GL_FLOAT, GL_FALSE },
            { 2, 2, GL_FLOAT, GL_FALSE }
        };

        std::shared_ptr<Geometry> planeGeo = std::make_shared<Geometry>(planeVertices, p_indices, f_attributes);
        std::shared_ptr<Texture> planeTex = std::make_shared <Texture>("../assets/textures/wood.png");
        std::vector<std::shared_ptr<Texture>> texs = { planeTex };
        plane = std::make_shared<Mesh>(*planeGeo, texs);
        return plane;
    }

    std::shared_ptr<Mesh> GetCube()
    {
        if (cube) return cube;

        std::vector<float> cube_vertices{
            // back face
            -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left   0
             1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right     1
             1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right  2       
            -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left      3
            // front face
            -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left   4
             1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right  5
             1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right     6
            -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left      7
            // left face
            -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right     8
            -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left      9
            -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left   10
            -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right  11
            // right face
             1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left      12
             1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right  13
             1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right     14  
             1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left   15
             // bottom face
             -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right    16
              1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left     17
              1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left  18
             -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right 19
             // top face
             -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left     20
              1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right 21
              1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right    22
             -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left  23
        };

        std::vector<unsigned int> cube_indices = {
            0, 1, 2,
            1, 0, 3,
            4, 5, 6,
            6, 7, 4,
            8, 9, 10,
            10, 11, 8,
            12, 13, 14,
            13, 12, 15,
            16, 17, 18,
            18, 19, 16,
            20, 21, 22,
            21, 20, 23
        };

        std::vector<VertexAttribute> cube_attributes = {
            { 0, 3, GL_FLOAT, GL_FALSE },
            { 1, 3, GL_FLOAT, GL_FALSE },
            { 2, 2, GL_FLOAT, GL_FALSE }
        };

        std::shared_ptr<Geometry> cubeGeo = std::make_shared<Geometry>(cube_vertices, cube_indices, cube_attributes);
        cube = std::make_shared<Mesh>(*cubeGeo);
        return cube;
    }

    static std::array<glm::mat4, 6> calculateCubeMatrices(glm::vec3 center, float nearZ = 0.1f, float farZ = 100.0f) {
        // The aspect of IBL and Shadow is always 1.0f.
        glm::mat4 projection = glm::perspective(glm::radians(90.0f), 1.0f, nearZ, farZ);

        return {
            projection * glm::lookAt(center, center + glm::vec3(1.0,  0.0,  0.0), glm::vec3(0.0, -1.0,  0.0)),
            projection * glm::lookAt(center, center + glm::vec3(-1.0, 0.0,  0.0), glm::vec3(0.0, -1.0,  0.0)),
            projection * glm::lookAt(center, center + glm::vec3(0.0,  1.0,  0.0), glm::vec3(0.0,  0.0,  1.0)),
            projection * glm::lookAt(center, center + glm::vec3(0.0, -1.0,  0.0), glm::vec3(0.0,  0.0, -1.0)),
            projection * glm::lookAt(center, center + glm::vec3(0.0,  0.0,  1.0), glm::vec3(0.0, -1.0,  0.0)),
            projection * glm::lookAt(center, center + glm::vec3(0.0,  0.0, -1.0), glm::vec3(0.0, -1.0,  0.0))
        };
    }

private:
    ResourceManager() = default;

    std::unordered_map<std::string, std::shared_ptr<Shader>> shaders;
    std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
    std::unordered_map<std::string, std::shared_ptr<Model>> models;
    std::unordered_map<std::string, std::shared_ptr<Material>> materials;

    std::shared_ptr<Mesh> screenQuad; // Cache a frequently used Quad
    std::shared_ptr<Mesh> plane;
    std::shared_ptr<Mesh> cube;
};