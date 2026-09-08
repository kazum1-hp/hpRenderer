#pragma once
#include "hpr/assets/EnvironmentAsset.h"
#include "hpr/renderer/ibl/IBLPrecompute.h"
#include <map>
#include <cstdint>

class Texture;
class AssetManager;

class IBLCache
{
public:
    explicit IBLCache(IBLSettings settings = {}) : precompute(settings) {}
    void initialize(AssetManager& resources);
    EnvironmentGpuView prepare(const std::shared_ptr<const EnvironmentAsset>& asset);
    void clear();
    std::size_t size() const { return entries.size(); }
private:
    struct Entry
    {
        std::shared_ptr<Texture> source;
        std::uint64_t revision = 0;
        std::array<GLuint, 4> programs{};
        std::unique_ptr<EnvironmentGpuResources> resources;
    };
    using Key = std::weak_ptr<const EnvironmentAsset>;
    std::map<Key, Entry, std::owner_less<Key>> entries;
    AssetManager* resources = nullptr;
    IBLPrecompute precompute;
};
