#include "hpr/assets/AssetManager.h"
#include "hpr/renderer/opengl/Shader.h"
#include "hpr/renderer/opengl/Texture.h"
#include "hpr/assets/Model.h"
#include <iostream>
#include <stdexcept>

AssetManager::~AssetManager() = default;
void AssetManager::Clear()
{
    models.clear();
    textures.clear();
    environments.clear();
    shaders.clear();
}
std::shared_ptr<Shader> AssetManager::LoadShader(ShaderId id, const std::string& vs, const std::string& fs,
                                                 const std::string& gs)
{
    if (static_cast<std::size_t>(id) >= ShaderNames.size())
        throw std::invalid_argument("Invalid shader id");
    if (auto cached = GetShader(id))
        return cached;
    auto shader = std::make_shared<Shader>(vs, fs, gs);
    GLint linked = GL_FALSE;
    if (shader->ID)
        glGetProgramiv(shader->ID, GL_LINK_STATUS, &linked);
    if (!linked)
        return nullptr;
    shaders.emplace(id, shader);
    return shader;
}
std::shared_ptr<Shader> AssetManager::GetShader(ShaderId id) const
{
    const auto it = shaders.find(id);
    return it == shaders.end() ? nullptr : it->second;
}
bool AssetManager::ReloadShader(ShaderId id)
{
    const auto shader = GetShader(id);
    return shader && shader->reload();
}
std::vector<std::pair<ShaderId, bool>> AssetManager::ReloadAllShaders()
{
    std::vector<std::pair<ShaderId, bool>> result;
    for (const auto& entry : shaders)
        result.emplace_back(entry.first, entry.second->reload());
    return result;
}
std::shared_ptr<Texture> AssetManager::LoadTexture(const std::string& path, TextureType semantic, ColorSpace colorSpace)
{
    const TextureKey key{NormalizeAssetPath(path), semantic, colorSpace};
    const auto it = textures.find(key);
    if (it != textures.end())
        return it->second;
    auto texture = std::make_shared<Texture>(key.path.generic_u8string(), semantic, colorSpace);
    if (!texture->isValid())
        return nullptr;
    textures.emplace(key, texture);
    return texture;
}
std::shared_ptr<Texture> AssetManager::ReloadTexture(const std::string& path, TextureType semantic,
                                                     ColorSpace colorSpace)
{
    const TextureKey key{NormalizeAssetPath(path), semantic, colorSpace};
    auto texture = std::make_shared<Texture>(key.path.generic_u8string(), semantic, colorSpace);
    if (!texture->isValid())
        return nullptr; // Keep the previous cached object on failure.
    textures[key] = texture;
    return texture;
}
std::shared_ptr<const EnvironmentAsset> AssetManager::LoadEnvironment(const std::string& path)
{
    const auto key = NormalizeAssetPath(path);
    if (!LoadTexture(key.generic_u8string(), HDR, ColorSpace::Linear))
        return nullptr;
    auto asset = environments[key].lock();
    if (!asset)
    {
        asset = std::make_shared<EnvironmentAsset>(key.generic_u8string());
        environments[key] = asset;
    }
    return asset;
}
std::shared_ptr<const EnvironmentAsset> AssetManager::ReloadEnvironment(const std::string& path)
{
    const auto key = NormalizeAssetPath(path);
    if (!ReloadTexture(key.generic_u8string(), HDR, ColorSpace::Linear))
        return nullptr;
    auto asset = environments[key].lock();
    if (asset)
        ++asset->revision;
    else
    {
        asset = std::make_shared<EnvironmentAsset>(key.generic_u8string());
        environments[key] = asset;
    }
    return asset;
}
std::shared_ptr<Texture> AssetManager::GetEnvironmentTexture(const EnvironmentAsset& asset)
{
    return LoadTexture(asset.getPath(), HDR, ColorSpace::Linear);
}
std::shared_ptr<Model> AssetManager::LoadModel(const std::string& path)
{
    const auto key = NormalizeAssetPath(path);
    const auto it = models.find(key);
    if (it != models.end())
        return it->second;
    try
    {
        auto model = std::make_shared<Model>(key.generic_u8string());
        if (!model->isValid())
            return nullptr;
        models.emplace(key, model);
        return model;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Model load failed: " << error.what() << '\n';
        return nullptr;
    }
}
bool AssetManager::ReloadModel(const std::string& path)
{
    const auto key = NormalizeAssetPath(path);
    const auto it = models.find(key);
    if (it == models.end())
        return static_cast<bool>(LoadModel(path));
    try
    {
        return it->second->reload(key.generic_u8string());
    }
    catch (const std::exception& error)
    {
        std::cerr << "Model reload failed: " << error.what() << '\n';
        return false;
    }
}
