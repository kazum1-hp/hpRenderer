#include "IBLCache.h"
#include "ResourceManager.h"
#include "Scene.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    struct Fixtures
    {
        std::filesystem::path directory;
        Fixtures()
        {
            directory = std::filesystem::temp_directory_path() /
                ("hpRenderer-ibl-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            require(std::filesystem::create_directory(directory), "create IBL fixture directory");
        }
        ~Fixtures()
        {
            std::error_code ignored;
            // Only this test's uniquely created directory is removed.
            std::filesystem::remove_all(directory, ignored);
        }
        std::string path(const char* name) const { return (directory / name).generic_string(); }
        void hdr(const char* name, unsigned char red = 128) const
        {
            std::ofstream file(directory / name, std::ios::binary);
            file << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 2\n";
            const unsigned char pixel[] = {red, 64, 32, 129};
            for (int i = 0; i < 4; ++i)
                file.write(reinterpret_cast<const char*>(pixel), sizeof(pixel));
            require(file.good(), "write HDR fixture");
        }
    };

    bool valid(EnvironmentGpuView view)
    {
        return glIsTexture(view.envCubemap) && glIsTexture(view.irradianceMap) &&
            glIsTexture(view.prefilterMap) && glIsTexture(view.brdfLUT);
    }

    void checkPixels(EnvironmentGpuView view, float red)
    {
        glBindTexture(GL_TEXTURE_CUBE_MAP, view.envCubemap);
        GLint width = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &width);
        require(width == 16, "environment cubemap dimensions");
        std::vector<float> pixels(static_cast<std::size_t>(width * width * 3));
        for (int face = 0; face < 6; ++face)
        {
            glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, GL_RGB, GL_FLOAT, pixels.data());
            require(std::isfinite(pixels[0]) && std::fabs(pixels[0] - red) < 0.03f,
                "converted HDR pixel mismatch");
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, view.irradianceMap);
        glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_TEXTURE_WIDTH, &width);
        require(width == 2, "irradiance dimensions");
        glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, GL_RGB, GL_FLOAT, pixels.data());
        require(std::isfinite(pixels[0]) && pixels[0] > 0.1f, "irradiance must be finite and nonblack");
        glBindTexture(GL_TEXTURE_CUBE_MAP, view.prefilterMap);
        for (int mip = 0; mip < 5; ++mip)
        {
            glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X, mip, GL_TEXTURE_WIDTH, &width);
            require(width == (16 >> mip), "prefilter mip dimensions");
            glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X, mip, GL_RGB, GL_FLOAT, pixels.data());
            require(std::isfinite(pixels[0]) && std::fabs(pixels[0] - red) < 0.05f, "prefilter pixels");
        }
        glBindTexture(GL_TEXTURE_2D, view.brdfLUT);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        require(width == 8, "BRDF dimensions");
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RG, GL_FLOAT, pixels.data());
        require(std::isfinite(pixels[0]) && pixels[0] > 0.0f, "BRDF pixels");
        glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    void gpuTests(std::shared_ptr<const EnvironmentAsset>& survivingAsset)
    {
        Fixtures fixtures;
        fixtures.hdr("a.hdr");
        fixtures.hdr("b.hdr", 64);
        auto& resources = ResourceManager::GetInstance();
        const std::string shaders = std::string(HPRENDERER_SOURCE_DIR) + "/shaders/";
        for (const char* name : {"skybox", "irradiance", "prefilter", "brdf"})
        {
            for (const char* extension : {".vs", ".fs"})
                std::filesystem::copy_file(shaders + name + extension, fixtures.directory / (std::string(name) + extension));
            resources.LoadShader(name, fixtures.path((std::string(name) + ".vs").c_str()),
                fixtures.path((std::string(name) + ".fs").c_str()));
        }
        IBLCache cache({16, 2, 16, 5, 8});
        cache.initialize(resources);
        auto a = resources.LoadEnvironment(fixtures.path("a.hdr"));
        auto b = resources.LoadEnvironment(fixtures.path("b.hdr"));
        require(a && b, "HDR assets loaded");
        require(resources.LoadEnvironment(fixtures.path("a.hdr")) == a, "shared asset identity");
        Scene first, second;
        first.SetEnvironment(a);
        second.SetEnvironment(a);

        // Deliberately unusual state must neither corrupt baking nor leak out.
        glViewport(3, 4, 31, 29);
        glEnable(GL_CULL_FACE);
        glEnable(GL_SCISSOR_TEST);
        glScissor(0, 0, 0, 0);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glColorMask(GL_FALSE, GL_TRUE, GL_FALSE, GL_TRUE);
        glActiveTexture(GL_TEXTURE3);
        auto firstMaps = cache.prepare(first.GetEnvironment().asset);
        require(valid(firstMaps), "initial bake failed");
        GLint viewport[4], activeTexture;
        GLboolean depthMask, colorMask[4];
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        require(viewport[0] == 3 && viewport[1] == 4 && viewport[2] == 31 && viewport[3] == 29,
            "bake changed viewport");
        require(activeTexture == GL_TEXTURE3 && glIsEnabled(GL_CULL_FACE) && glIsEnabled(GL_SCISSOR_TEST)
            && glIsEnabled(GL_BLEND) && !glIsEnabled(GL_DEPTH_TEST), "bake leaked GL state");
        require(!depthMask && !colorMask[0] && colorMask[1] && !colorMask[2] && colorMask[3], "bake leaked write masks");
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glActiveTexture(GL_TEXTURE0);
        checkPixels(firstMaps, 1.0f);
        require(cache.prepare(a).envCubemap == firstMaps.envCubemap, "unchanged asset rebaked");
        require(cache.prepare(second.GetEnvironment().asset).envCubemap == firstMaps.envCubemap,
            "scenes sharing an asset duplicated IBL");
        first.Clear();
        require(valid(firstMaps), "clearing a scene deleted another scene's maps");
        auto secondMaps = cache.prepare(b);
        require(valid(secondMaps) && secondMaps.envCubemap != firstMaps.envCubemap, "separate asset cache");
        require(secondMaps.brdfLUT == firstMaps.brdfLUT, "BRDF LUT not shared");
        checkPixels(secondMaps, 0.5f);

        fixtures.hdr("a.hdr", 192);
        const auto revision = a->getRevision();
        require(resources.ReloadEnvironment(fixtures.path("a.hdr")) == a, "reload changed asset identity");
        require(a->getRevision() == revision + 1 && second.GetEnvironment().asset->getRevision() == revision + 1,
            "shared revision was not updated");
        auto reloaded = cache.prepare(a);
        require(valid(reloaded) && reloaded.envCubemap != firstMaps.envCubemap, "reload did not rebake");
        require(!glIsTexture(firstMaps.envCubemap) && reloaded.brdfLUT == firstMaps.brdfLUT, "replacement/LUT lifecycle");
        checkPixels(reloaded, 1.5f);
        {
            std::ofstream invalid(fixtures.directory / "a.hdr", std::ios::binary);
            invalid << "invalid HDR";
        }
        require(!resources.ReloadEnvironment(fixtures.path("a.hdr")), "invalid HDR reload accepted");
        require(a->getRevision() == revision + 1 && cache.prepare(a).envCubemap == reloaded.envCubemap,
            "failed HDR reload destroyed prior resources");

        // A failed bake must retain the last complete maps without retrying every frame.
        auto prefilter = resources.GetShader("prefilter");
        const GLuint savedProgram = prefilter->ID;
        prefilter->ID = 0;
        auto failed = cache.prepare(a);
        require(failed.envCubemap == reloaded.envCubemap, "failed bake lost old maps");
        require(cache.prepare(a).envCubemap == reloaded.envCubemap, "failed bake cache mismatch");
        prefilter->ID = savedProgram;
        auto recovered = cache.prepare(a);
        require(valid(recovered) && recovered.envCubemap != reloaded.envCubemap, "bake did not recover");
        glUseProgram(0);
        const auto brdfPath = fixtures.directory / "brdf.fs";
        std::filesystem::last_write_time(brdfPath,
            std::filesystem::last_write_time(brdfPath) + std::chrono::seconds(2));
        require(resources.GetShader("brdf")->reload(), "BRDF shader reload failed");
        auto newBrdf = cache.prepare(a);
        require(valid(newBrdf) && newBrdf.brdfLUT != recovered.brdfLUT, "BRDF reload did not invalidate LUT");
        require(cache.prepare(b).brdfLUT == newBrdf.brdfLUT, "new LUT not shared");
        require(!glIsTexture(recovered.brdfLUT), "obsolete LUT leaked");
        auto bMaps = cache.prepare(b);
        b.reset();
        cache.prepare(nullptr); // Also collects unreferenced asset entries.
        require(cache.size() == 1 && !glIsTexture(bMaps.envCubemap), "unused environment not collected");
        require(cache.prepare(nullptr).envCubemap == 0, "empty scene inherited environment");

        survivingAsset = a;
        cache.clear();
        cache.clear();
        require(!glIsTexture(newBrdf.envCubemap) && !glIsTexture(newBrdf.brdfLUT), "cache cleanup leaked GPU maps");
        resources.Clear();
        require(a->getPath() == fixtures.path("a.hdr"), "clearing GPU resources damaged scene metadata");
        require(glGetError() == GL_NO_ERROR, "IBL tests left OpenGL errors");
    }
}

int main()
{
    GLFWwindow* window = nullptr;
    std::shared_ptr<const EnvironmentAsset> survivingAsset;
    int result = 0;
    try
    {
        require(glfwInit() == GLFW_TRUE, "GLFW init");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(64, 64, "IBL tests", nullptr, nullptr);
        require(window != nullptr, "hidden context creation");
        glfwMakeContextCurrent(window);
        require(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0, "GL loader");
        gpuTests(survivingAsset);
        std::cout << "IBL GPU lifecycle tests passed.\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    if (window)
    {
        ResourceManager::GetInstance().Clear();
        glfwDestroyWindow(window);
    }
    glfwTerminate();
    survivingAsset.reset(); // CPU metadata can safely outlive the graphics context.
    return result;
}
