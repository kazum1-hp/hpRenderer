#pragma once
#include <glad/glad.h>
#include <array>
#include <memory>

class Shader;
class Mesh;
class Texture;
class ResourceManager;

struct EnvironmentGpuView
{
    GLuint envCubemap = 0;
    GLuint irradianceMap = 0;
    GLuint prefilterMap = 0;
    GLuint brdfLUT = 0;
};

// Owned exclusively by the rendering cache. Destroy with its GL context current.
struct EnvironmentGpuResources
{
    GLuint envCubemap = 0;
    GLuint irradianceMap = 0;
    GLuint prefilterMap = 0;
    std::shared_ptr<Texture> brdfLUT;
    EnvironmentGpuResources() = default;
    ~EnvironmentGpuResources();
    EnvironmentGpuResources(const EnvironmentGpuResources&) = delete;
    EnvironmentGpuResources& operator=(const EnvironmentGpuResources&) = delete;
    EnvironmentGpuView view() const;
};

struct IBLSettings
{
    unsigned int environmentSize = 1024;
    unsigned int irradianceSize = 32;
    unsigned int prefilterSize = 128;
    unsigned int prefilterMipLevels = 5;
    unsigned int brdfSize = 512;
};

class IBLPrecompute
{
public:
    explicit IBLPrecompute(IBLSettings settings = {}) : settings(settings) {}
    void initialize(ResourceManager& resources);
    void shutdown();
    std::array<GLuint, 4> programIds() const;
    // Transactional: incomplete allocations are released on failure.
    std::unique_ptr<EnvironmentGpuResources> bake(GLuint hdrTexture);
private:
    IBLSettings settings;
    std::shared_ptr<Shader> skyboxShader, irradianceShader, prefilterShader, brdfShader;
    std::shared_ptr<Mesh> cube, screenQuad;
    std::shared_ptr<Texture> cachedBrdfLUT;
    GLuint cachedBrdfProgram = 0;
};
