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
    asset = std::move(replacement);
    ++revision;
    return true;
}
