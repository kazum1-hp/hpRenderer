#pragma once
#include "hpr/assets/ImageData.h"
#include <array>
#include <memory>
#include <string>
enum class EnvironmentMode
{
    IBL,
    SixFaces,
    Disabled
};
struct SkyboxAsset
{
    // OpenGL order: +X, -X, +Y, -Y, +Z, -Z. No automatic image rotation/flip.
    std::array<std::string, 6> paths;
    std::array<ImageData, 6> faces;
    static std::shared_ptr<const SkyboxAsset> Load(const std::array<std::string, 6>& paths, std::string& error);
    // Explicit UI action only: reorder a complete set named px/nx/py/ny/pz/nz.
    // Never infer orientation or silently rearrange arbitrary filenames during Load.
    static bool MatchNamedFaces(const std::array<std::string, 6>& paths,
                                std::array<std::string, 6>& ordered, std::string& error);
};
