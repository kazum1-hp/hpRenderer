#include "hpr/renderer/opengl/GpuModel.h"
#include <iostream>
#include <map>
#include <tuple>
#include <functional>
GpuModel::GpuModel(const Model& model, TextureLoader& loader, bool refreshTextures)
    : source(model.snapshot()), revision(model.getRevision()), materials(source->materials)
{
    const auto& asset = *source;
    std::map<TextureKey, std::shared_ptr<Texture>, std::function<bool(const TextureKey&, const TextureKey&)>> files(
        [](const auto& a, const auto& b) {
            return std::tie(a.path, a.semantic, a.colorSpace) < std::tie(b.path, b.semantic, b.colorSpace);
        });
    // Embedded image identity is scoped to this imported asset revision, not a fake file path.
    std::map<std::pair<int, TextureType>, std::shared_ptr<Texture>> embedded;
    const std::vector<VertexAttribute> attributes = {{0, 3, GL_FLOAT, GL_FALSE},
                                                     {1, 3, GL_FLOAT, GL_FALSE},
                                                     {2, 2, GL_FLOAT, GL_FALSE},
                                                     {3, 3, GL_FLOAT, GL_FALSE},
                                                     {4, 3, GL_FLOAT, GL_FALSE}};
    for (const auto& source : asset.meshes)
    {
        std::vector<std::shared_ptr<Texture>> textures;
        auto& material = materials.at(source.materialIndex);
        for (const auto& ref : material.textures)
        {
            std::shared_ptr<Texture> texture;
            if (ref.embeddedImage >= 0)
            {
                auto& cached = embedded[{ref.embeddedImage, ref.semantic}];
                if (!cached)
                {
                    const auto& image = asset.images.at(ref.embeddedImage);
                    cached = std::make_shared<Texture>(ref.path, image.bytes, image.width, image.height, ref.semantic,
                                                       DefaultColorSpace(ref.semantic));
                }
                texture = cached;
            }
            else
            {
                TextureKey key{NormalizeAssetPath(ref.path), ref.semantic, DefaultColorSpace(ref.semantic)};
                auto& cached = files[key];
                if (!cached)
                {
                    if (refreshTextures)
                        cached = loader.ReloadTexture(ref.path, ref.semantic, key.colorSpace);
                    // A transient file failure must not discard a previously valid cache entry.
                    if (!cached)
                        cached = loader.LoadTexture(ref.path, ref.semantic, key.colorSpace);
                }
                texture = cached;
            }
            if (texture && texture->isValid())
            {
                if (material.inferAlphaFromTextures && (ref.semantic == Diffuse || ref.semantic == Opacity) &&
                    texture->hasTransparency())
                    material.alphaMode = AlphaMode::Blend;
                textures.push_back(std::move(texture));
            }
            else
                std::cerr << "[Model] Texture unavailable, using material fallback: " << ref.path << '\n';
        }
        meshes.push_back(std::make_unique<Mesh>(Geometry(source.vertices, source.indices, attributes), textures));
    }
}
