#pragma once
#include <filesystem>

// Existing runtime paths are relative to bin/ (or build/Debug and build/Release).
// Make launching from a terminal, Explorer, and an installed ZIP consistent.
void SetExecutableWorkingDirectory();

// Development executables use editable sources; relocated bundles use their own shaders.
std::filesystem::path ResolveShaderDirectory(const std::filesystem::path& executableDirectory);
