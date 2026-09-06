#pragma once
#include <cstdint>
#include <string>
#include <utility>

// CPU-only, serializable selection metadata. No OpenGL context is needed to
// create, share or destroy an environment asset.
class EnvironmentAsset
{
public:
    explicit EnvironmentAsset(std::string hdrPath) : path(std::move(hdrPath)) {}
    const std::string& getPath() const { return path; }
    std::uint64_t getRevision() const { return revision; }
private:
    friend class ResourceManager;
    const std::string path;
    std::uint64_t revision = 1;
};
