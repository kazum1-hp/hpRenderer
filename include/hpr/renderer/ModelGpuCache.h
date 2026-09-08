#pragma once
#include "hpr/renderer/RenderScene.h"
#include "hpr/renderer/opengl/GpuModel.h"
#include <map>
class ModelGpuCache
{
public:
    explicit ModelGpuCache(TextureLoader& loader) : loader(loader)
    {
    }
    GpuRenderScene prepare(const RenderScene& scene);
    void clear()
    {
        entries.clear();
    }

private:
    struct Entry
    {
        std::uint64_t revision = 0;
        std::shared_ptr<GpuModel> model;
    };
    using Key = std::weak_ptr<const Model>;
    std::map<Key, Entry, std::owner_less<Key>> entries;
    TextureLoader& loader;
};
