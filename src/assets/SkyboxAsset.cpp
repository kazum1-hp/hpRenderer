#include "hpr/assets/SkyboxAsset.h"
#include <filesystem>
#include <algorithm>
#include <cctype>

bool SkyboxAsset::MatchNamedFaces(const std::array<std::string, 6>& paths,
                                std::array<std::string, 6>& ordered, std::string& error)
{
    const std::array<std::string, 6> names{"px", "nx", "py", "ny", "pz", "nz"};
    std::array<std::string, 6> result;
    error.clear();
    for (const auto& path : paths)
    {
        auto name = std::filesystem::u8path(path).stem().u8string();
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::tolower(c); });
        const auto found = std::find(names.begin(), names.end(), name);
        if (found == names.end() || !result[found - names.begin()].empty())
        {
            error = "Select one file each named px, nx, py, ny, pz, nz before matching; paths were not changed.";
            return false;
        }
        result[found - names.begin()] = path;
    }
    ordered = std::move(result);
    return true;
}

std::shared_ptr<const SkyboxAsset> SkyboxAsset::Load(const std::array<std::string, 6>& paths, std::string& error)
{
    auto asset = std::make_shared<SkyboxAsset>();
    error.clear();
    for (size_t i = 0; i < 6; ++i)
    {
        asset->paths[i] = std::filesystem::absolute(std::filesystem::u8path(paths[i])).generic_u8string();
        asset->faces[i] = ImageData::Load(asset->paths[i]);
        const auto& face = asset->faces[i];
        if (!face.valid() || face.width != face.height || face.width != asset->faces[0].width)
        {
            error = "Skybox requires six readable square images of equal size; invalid face: " + paths[i];
            return nullptr;
        }
    }
    return asset;
}
