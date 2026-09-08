#pragma once
#include "hpr/assets/TextureTypes.h"
#include <memory>
#include <string>
class Texture;

// GPU upload borrows this service; CPU Model import never calls it.
// GpuModel retains shared textures, not the loader or its owner.
class TextureLoader
{
public:
    virtual ~TextureLoader() = default;
    virtual std::shared_ptr<Texture> LoadTexture(const std::string& path, TextureType semantic,
                                                 ColorSpace colorSpace) = 0;
    virtual std::shared_ptr<Texture> ReloadTexture(const std::string& path, TextureType semantic, ColorSpace space)
    { return LoadTexture(path, semantic, space); }
    std::shared_ptr<Texture> LoadTexture(const std::string& path, TextureType semantic = Diffuse)
    {
        return LoadTexture(path, semantic, DefaultColorSpace(semantic));
    }
};
