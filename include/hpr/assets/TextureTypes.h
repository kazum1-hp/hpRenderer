#pragma once
#include <filesystem>
#include <functional>

enum TextureType
{
    Diffuse,
    Specular,
    Normal,
    Height,
    ARM,
    HDR,
    AttachMent,
    AmbientOcclusion,
    Roughness,
    Metallic,
    Opacity
};
using TextureSemantic = TextureType;
enum class ColorSpace
{
    Linear,
    SRGB
};

constexpr ColorSpace DefaultColorSpace(TextureType semantic)
{
    return semantic == Diffuse ? ColorSpace::SRGB : ColorSpace::Linear;
}

// UTF-8 at API boundaries, absolute lexical paths in caches. No case folding or
// symlink resolution: identity must not change when a failed file later appears.
inline std::filesystem::path NormalizeAssetPath(const std::string& path)
{
    return std::filesystem::absolute(std::filesystem::u8path(path)).lexically_normal();
}
struct TextureKey
{
    std::filesystem::path path;
    TextureSemantic semantic;
    ColorSpace colorSpace;
    bool operator==(const TextureKey& other) const
    {
        return path == other.path && semantic == other.semantic && colorSpace == other.colorSpace;
    }
};
struct TextureKeyHash
{
    std::size_t operator()(const TextureKey& key) const
    {
        auto value = std::filesystem::hash_value(key.path);
        value ^= static_cast<std::size_t>(key.semantic) + 0x9e3779b9 + (value << 6) + (value >> 2);
        return value ^ (static_cast<std::size_t>(key.colorSpace) + 0x9e3779b9 + (value << 6) + (value >> 2));
    }
};
