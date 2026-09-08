#pragma once
#include "hpr/assets/TextureLoader.h"
#include "hpr/renderer/ShaderId.h"
#include "hpr/assets/EnvironmentAsset.h"
#include <unordered_map>
#include <vector>
#include <utility>

class Shader;
class Model;

// Application-owned cache. Borrowers and GPU resources must be released while
// their context is current. Empty construction/destruction needs no GL context.
class AssetManager final : public TextureLoader
{
public:
    AssetManager() = default;
    ~AssetManager() override;
    AssetManager(const AssetManager&) = delete;
    AssetManager& operator=(const AssetManager&) = delete;
    void Clear();
    std::shared_ptr<Shader> LoadShader(ShaderId id, const std::string& vsPath, const std::string& fsPath,
                                       const std::string& gsPath = "");
    std::shared_ptr<Shader> GetShader(ShaderId id) const;
    bool ReloadShader(ShaderId id);
    std::vector<std::pair<ShaderId, bool>> ReloadAllShaders();

    using TextureLoader::LoadTexture;
    std::shared_ptr<Texture> LoadTexture(const std::string& path, TextureType semantic, ColorSpace colorSpace) override;
    // Replaces only this cache variant; existing holders keep their texture.
    std::shared_ptr<Texture> ReloadTexture(const std::string& path, TextureType semantic, ColorSpace colorSpace) override;
    std::shared_ptr<Texture> ReloadTexture(const std::string& path, TextureType semantic = Diffuse)
    {
        return ReloadTexture(path, semantic, DefaultColorSpace(semantic));
    }
    std::shared_ptr<const EnvironmentAsset> LoadEnvironment(const std::string& path);
    std::shared_ptr<const EnvironmentAsset> ReloadEnvironment(const std::string& path);
    std::shared_ptr<Texture> GetEnvironmentTexture(const EnvironmentAsset& asset);

    std::shared_ptr<Model> LoadModel(const std::string& path);
    bool ReloadModel(const std::string& path);

private:
    std::unordered_map<ShaderId, std::shared_ptr<Shader>> shaders;
    std::unordered_map<TextureKey, std::shared_ptr<Texture>, TextureKeyHash> textures;
    std::unordered_map<std::filesystem::path, std::weak_ptr<EnvironmentAsset>> environments;
    std::unordered_map<std::filesystem::path, std::shared_ptr<Model>> models;
};
