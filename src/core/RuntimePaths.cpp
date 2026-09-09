#include "hpr/core/RuntimePaths.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

void SetExecutableWorkingDirectory()
{
    std::vector<wchar_t> path(32768);
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0 || length >= path.size())
        throw std::runtime_error("Cannot resolve the executable directory");
    std::filesystem::current_path(std::filesystem::path(std::wstring(path.data(), length)).parent_path());
}
