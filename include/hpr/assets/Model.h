#pragma once
#include "hpr/assets/ModelAsset.h"
#include "hpr/assets/Bounds.h"
#include <memory>
#include <cstdint>

// CPU asset only; safe to import, retain and release without an OpenGL context.
class Model
{
public:
    explicit Model(const std::string& path);
    bool reload(const std::string& path);
    bool isValid() const
    {
        return asset && !asset->draws.empty();
    }
    const std::string& getPath() const
    {
        return data().path;
    }
    const ModelAsset& data() const
    {
        return *asset;
    }
    std::shared_ptr<const ModelAsset> snapshot() const
    {
        return asset;
    }
    std::uint64_t getRevision() const
    {
        return revision;
    }
    const Bounds& getBounds() const { return bounds; }

private:
    std::shared_ptr<const ModelAsset> asset = std::make_shared<ModelAsset>();
    std::uint64_t revision = 0;
    Bounds bounds;
};
