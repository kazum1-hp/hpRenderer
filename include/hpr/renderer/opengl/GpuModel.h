#pragma once
#include "hpr/assets/Model.h"
#include "hpr/renderer/opengl/Mesh.h"
#include "hpr/assets/TextureLoader.h"
class GpuModel
{
public:
    GpuModel(const Model& model, TextureLoader& loader, bool refreshTextures = false);
    const ModelAsset& data() const
    {
        return *source;
    }
    const ModelMaterial& material(size_t index) const
    {
        return materials.at(index);
    }
    std::uint64_t getRevision() const
    {
        return revision;
    }
    const std::string& getPath() const
    {
        return source->path;
    }
    const Mesh& mesh(size_t index) const
    {
        return *meshes.at(index);
    }
    size_t meshCount() const
    {
        return meshes.size();
    }
    bool isValid() const
    {
        return !meshes.empty();
    }

private:
    std::shared_ptr<const ModelAsset> source;
    std::uint64_t revision;
    std::vector<ModelMaterial> materials;
    std::vector<std::unique_ptr<Mesh>> meshes;
};
