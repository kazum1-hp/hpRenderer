#pragma once
#include "hpr/assets/SkyboxAsset.h"
#include <glad/glad.h>

// Six-image skybox GPU resource. Rendering stays in SkyboxPass.
class Skybox
{
public:
    Skybox() = default;
    ~Skybox();
    Skybox(const Skybox&) = delete;
    Skybox& operator=(const Skybox&) = delete;
    bool load(const SkyboxAsset& asset);
    bool LoadFromFiles(const std::vector<std::string>& paths);
    GLuint getID() const
    {
        return id;
    }

private:
    GLuint id = 0;
};
