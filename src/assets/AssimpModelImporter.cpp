#include "hpr/assets/AssimpModelImporter.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>
#include <algorithm>
#include <cctype>
#include <functional>
#include <limits>
#include <stdexcept>

namespace
{
glm::mat4 Matrix(const aiMatrix4x4& m)
{
    // Assimp fields are rows; GLM's first subscript/constructor column is a column.
    return glm::mat4(m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2, m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4);
}
void Warn(ModelAsset& asset, const std::string& warning)
{
    if (std::find(asset.warnings.begin(), asset.warnings.end(), warning) == asset.warnings.end())
        asset.warnings.push_back(warning);
}
} // namespace

bool AssimpModelImporter::Import(const std::string& path, ModelAsset& output, std::string& error)
{
    error.clear();
    try
    {
        std::string extension = std::filesystem::u8path(path).extension().u8string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".blend")
            throw std::runtime_error("Open .blend in Blender and export static glTF 2.0 (.glb/.gltf) first.");

        Assimp::Importer importer;
        // One UV flip for our top-down image upload convention. The PMX reader's own
        // conversion produces Assimp UVs; this postprocess brings them to our convention.
        const aiScene* scene =
            importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenSmoothNormals |
                                        aiProcess_CalcTangentSpace | aiProcess_ValidateDataStructure);
        if (!scene || !scene->mRootNode || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE))
            throw std::runtime_error(importer.GetErrorString());
        ModelAsset result;
        result.path = path;
        const auto directory = std::filesystem::u8path(path).parent_path();
        const bool gltf = extension == ".gltf" || extension == ".glb";
        if (scene->HasAnimations())
            Warn(result, "Animations are not evaluated; importing static mesh data only.");

        for (unsigned int i = 0; i < scene->mNumTextures; ++i)
        {
            const aiTexture& image = *scene->mTextures[i];
            ModelImage copy;
            if (image.mHeight == 0)
            {
                const auto* bytes = reinterpret_cast<const unsigned char*>(image.pcData);
                copy.bytes.assign(bytes, bytes + image.mWidth);
            }
            else
            {
                copy.width = image.mWidth;
                copy.height = image.mHeight;
                const std::size_t count = static_cast<std::size_t>(copy.width) * copy.height;
                if (count > std::numeric_limits<std::size_t>::max() / 4)
                    throw std::runtime_error("Embedded image is too large.");
                copy.bytes.reserve(count * 4);
                for (std::size_t j = 0; j < count; ++j)
                {
                    const aiTexel& p = image.pcData[j];
                    copy.bytes.insert(copy.bytes.end(), {p.r, p.g, p.b, p.a});
                }
            }
            result.images.push_back(std::move(copy));
        }
        for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
        {
            const aiMaterial& source = *scene->mMaterials[i];
            ModelMaterial material;
            aiString name;
            source.Get(AI_MATKEY_NAME, name);
            material.name = name.C_Str();
            aiColor4D color(1, 1, 1, 1);
            if (source.Get(AI_MATKEY_BASE_COLOR, color) != AI_SUCCESS)
            {
                source.Get(AI_MATKEY_COLOR_DIFFUSE, color);
                float opacity = color.a;
                source.Get(AI_MATKEY_OPACITY, opacity);
                color.a = opacity;
            }
            material.baseColor = {color.r, color.g, color.b, color.a};
            material.roughness = gltf ? 1.0f : 0.5f;
            material.metallic = gltf ? 1.0f : 0.0f;
            source.Get(AI_MATKEY_ROUGHNESS_FACTOR, material.roughness);
            source.Get(AI_MATKEY_METALLIC_FACTOR, material.metallic);
            int twoSided = 0;
            source.Get(AI_MATKEY_TWOSIDED, twoSided);
            material.doubleSided = twoSided != 0;
            aiString mode;
            if (source.Get(AI_MATKEY_GLTF_ALPHAMODE, mode) == AI_SUCCESS)
            {
                if (std::string(mode.C_Str()) == "MASK")
                    material.alphaMode = AlphaMode::Mask;
                if (std::string(mode.C_Str()) == "BLEND")
                    material.alphaMode = AlphaMode::Blend;
            }
            else if (color.a < 1.0f)
                material.alphaMode = AlphaMode::Blend;
            source.Get(AI_MATKEY_GLTF_ALPHACUTOFF, material.alphaCutoff);

            auto texture = [&](aiTextureType type, TextureType semantic, int channel = 0) {
                if (!source.GetTextureCount(type))
                    return false;
                aiString reference;
                unsigned int uv = 0;
                if (source.GetTexture(type, 0, &reference, nullptr, &uv) != AI_SUCCESS)
                    return false;
                if (uv != 0)
                {
                    Warn(result, "Only UV0 is supported; a texture using another UV set was skipped.");
                    return false;
                }
                ModelTexture entry;
                entry.semantic = semantic;
                entry.channel = channel;
                const aiTexture* embedded = scene->GetEmbeddedTexture(reference.C_Str());
                if (embedded)
                {
                    for (unsigned int j = 0; j < scene->mNumTextures; ++j)
                        if (scene->mTextures[j] == embedded)
                            entry.embeddedImage = static_cast<int>(j);
                    entry.path = path + "#image" + std::to_string(entry.embeddedImage);
                }
                else
                {
                    std::string relative = reference.C_Str();
                    std::replace(relative.begin(), relative.end(), '\\', '/');
                    entry.path = (directory / std::filesystem::u8path(relative)).lexically_normal().generic_u8string();
                    // Compatibility with existing project glTF assets whose packed ORM image
                    // is named *_arm_* on disk but still referenced as *_rough_* in the glTF.
                    if (gltf && (semantic == Roughness || semantic == Metallic || semantic == AmbientOcclusion) &&
                        !std::filesystem::exists(std::filesystem::u8path(entry.path)))
                    {
                        auto file = std::filesystem::u8path(entry.path);
                        auto name = file.filename().u8string();
                        const auto token = name.find("_rough_");
                        if (token != std::string::npos)
                        {
                            name.replace(token, 7, "_arm_");
                            const auto fallback = file.parent_path() / std::filesystem::u8path(name);
                            if (std::filesystem::exists(fallback))
                            {
                                Warn(result,
                                     "Legacy glTF image alias: " + entry.path + " -> " + fallback.generic_u8string());
                                entry.path = fallback.generic_u8string();
                            }
                        }
                    }
                    if (!std::filesystem::exists(std::filesystem::u8path(entry.path)))
                        Warn(result, "Missing texture: " + entry.path);
                }
                material.textures.push_back(std::move(entry));
                return true;
            };
            if (!texture(aiTextureType_BASE_COLOR, Diffuse))
                texture(aiTextureType_DIFFUSE, Diffuse);
            texture(aiTextureType_SPECULAR, Specular);
            texture(aiTextureType_NORMALS, Normal);
            texture(aiTextureType_HEIGHT, Height);
            texture(aiTextureType_OPACITY, Opacity);
            if (!texture(aiTextureType_AMBIENT_OCCLUSION, AmbientOcclusion))
                texture(aiTextureType_LIGHTMAP, AmbientOcclusion);
            if (gltf && source.GetTextureCount(aiTextureType_GLTF_METALLIC_ROUGHNESS))
            {
                texture(aiTextureType_GLTF_METALLIC_ROUGHNESS, Roughness, 1);
                texture(aiTextureType_GLTF_METALLIC_ROUGHNESS, Metallic, 2);
            }
            else
            {
                texture(aiTextureType_DIFFUSE_ROUGHNESS, Roughness, gltf ? 1 : 0);
                texture(aiTextureType_METALNESS, Metallic, gltf ? 2 : 0);
            }
            // Resolve legacy texture alpha after image decode. Solid PMX skin/clothing
            // must still write depth rather than turning every material transparent.
            material.inferAlphaFromTextures = !gltf;
            result.materials.push_back(std::move(material));
        }
        for (unsigned int i = 0; i < scene->mNumMeshes; ++i)
        {
            const aiMesh& mesh = *scene->mMeshes[i];
            if (mesh.HasBones())
                Warn(result, "Skinning is not evaluated; use Blender to bake a specific pose to a static mesh.");
            if (mesh.mNumAnimMeshes)
                Warn(result, "Morph targets are not evaluated.");
            ModelMeshData data;
            data.hasTangents = mesh.HasTangentsAndBitangents();
            data.materialIndex = mesh.mMaterialIndex;
            if (data.materialIndex >= result.materials.size())
                throw std::runtime_error("Invalid mesh material.");
            glm::vec3 low(std::numeric_limits<float>::max()), high(std::numeric_limits<float>::lowest());
            for (unsigned int v = 0; v < mesh.mNumVertices; ++v)
            {
                const auto p = mesh.mVertices[v];
                const auto n = mesh.HasNormals() ? mesh.mNormals[v] : aiVector3D(0, 1, 0);
                const auto uv = mesh.HasTextureCoords(0) ? mesh.mTextureCoords[0][v] : aiVector3D();
                const auto t = mesh.HasTangentsAndBitangents() ? mesh.mTangents[v] : aiVector3D();
                const auto b = mesh.HasTangentsAndBitangents() ? mesh.mBitangents[v] : aiVector3D();
                data.vertices.insert(data.vertices.end(),
                                     {p.x, p.y, p.z, n.x, n.y, n.z, uv.x, uv.y, t.x, t.y, t.z, b.x, b.y, b.z});
                low = glm::min(low, glm::vec3(p.x, p.y, p.z));
                high = glm::max(high, glm::vec3(p.x, p.y, p.z));
            }
            for (unsigned int f = 0; f < mesh.mNumFaces; ++f)
            {
                const auto& face = mesh.mFaces[f];
                if (face.mNumIndices != 3)
                    continue; // Ignore points and lines in surface imports.
                for (unsigned int j = 0; j < 3; ++j)
                {
                    if (face.mIndices[j] >= mesh.mNumVertices)
                        throw std::runtime_error("Invalid vertex index.");
                    data.indices.push_back(face.mIndices[j]);
                }
            }
            data.center = (low + high) * 0.5f;
            result.meshes.push_back(std::move(data));
        }
        std::function<void(const aiNode&, int)> visit = [&](const aiNode& node, int parent) {
            const std::size_t index = result.nodes.size();
            const glm::mat4 local = Matrix(node.mTransformation);
            result.nodes.push_back(
                {node.mName.C_Str(), parent, local, parent < 0 ? local : result.nodes[parent].worldTransform * local});
            for (unsigned int i = 0; i < node.mNumMeshes; ++i)
            {
                const auto mesh = node.mMeshes[i];
                if (mesh >= result.meshes.size())
                    throw std::runtime_error("Invalid node mesh reference.");
                if (!result.meshes[mesh].indices.empty())
                    result.draws.push_back({mesh, index});
            }
            for (unsigned int i = 0; i < node.mNumChildren; ++i)
                visit(*node.mChildren[i], static_cast<int>(index));
        };
        visit(*scene->mRootNode, -1);
        if (result.draws.empty())
            throw std::runtime_error("No triangle meshes are referenced by the node tree.");
        output = std::move(result);
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}
