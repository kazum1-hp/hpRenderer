#include "hpr/core/RuntimePaths.h"
#include <iostream>
#include <stdexcept>

int main()
{
    try
    {
        namespace fs = std::filesystem;
        const auto build = fs::u8path(HPRENDERER_BUILD_DIR);
        const auto source = fs::u8path(HPRENDERER_SOURCE_SHADER_DIR);
        for (const char* configuration : {"Debug", "Release", "bin"})
            if (ResolveShaderDirectory(build / configuration) != source)
                throw std::runtime_error("Development build is reading copied shaders");
        // A package on the developer's machine must still use its bundled files,
        // even while the original source tree exists.
        const auto package = build / "shader-path-test-package";
        if (ResolveShaderDirectory(package / "bin") !=
            (fs::weakly_canonical(package) / "shaders"))
            throw std::runtime_error("Relocated package is reading development shaders");
        const auto original = fs::current_path();
        fs::current_path(fs::temp_directory_path());
        const auto resolved = ResolveShaderDirectory(build / "bin");
        fs::current_path(original);
        if (resolved != source)
            throw std::runtime_error("Shader directory depends on caller working directory");
        std::cout << "Development, relocated bundle and working-directory shader paths passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
