#include "hpr/assets/Model.h"
#include "hpr/assets/AssimpModelImporter.h"
#include <iostream>
Model::Model(const std::string& path)
{
    reload(path);
}
bool Model::reload(const std::string& path)
{
    auto replacement = std::make_shared<ModelAsset>();
    std::string error;
    if (!AssimpModelImporter::Import(path, *replacement, error))
    {
        std::cerr << "Model import failed, keeping previous asset: " << path << ": " << error << '\n';
        return false;
    }
    for (const auto& warning : replacement->warnings)
        std::cerr << "[Model] " << warning << '\n';
    Bounds replacementBounds;
    for (const auto& draw : replacement->draws)
    {
        const auto& vertices = replacement->meshes[draw.meshIndex].vertices;
        const auto& transform = replacement->nodes[draw.nodeIndex].worldTransform;
        for (std::size_t i = 0; i + 2 < vertices.size(); i += 14)
            replacementBounds.include(glm::vec3(transform *
                glm::vec4(vertices[i], vertices[i + 1], vertices[i + 2], 1.0f)));
    }
    bounds = replacementBounds;
    asset = std::move(replacement);
    ++revision;
    return true;
}
