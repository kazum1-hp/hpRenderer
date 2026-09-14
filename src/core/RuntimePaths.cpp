#include "hpr/core/RuntimePaths.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

std::filesystem::path ResolveShaderDirectory(const std::filesystem::path& executableDirectory)
{
    const auto directory = std::filesystem::weakly_canonical(executableDirectory);
#if defined(HPRENDERER_BUILD_DIR) && defined(HPRENDERER_SOURCE_SHADER_DIR)
    const auto buildDirectory = std::filesystem::weakly_canonical(
        std::filesystem::u8path(HPRENDERER_BUILD_DIR));
    const auto sourceShaders = std::filesystem::u8path(HPRENDERER_SOURCE_SHADER_DIR);
    // Supported layouts are build/{Debug,Release} and build/bin. Never let an
    // installed copy silently load shaders from the developer's checkout.
    if (directory.parent_path() == buildDirectory && std::filesystem::is_directory(sourceShaders))
        return sourceShaders;
#endif
    return (directory / "../shaders").lexically_normal();
}

void SetExecutableWorkingDirectory()
{
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
        throw std::runtime_error("Cannot resolve the executable directory");
    std::filesystem::current_path(std::filesystem::path(std::wstring(path.data(), length)).parent_path());
}
