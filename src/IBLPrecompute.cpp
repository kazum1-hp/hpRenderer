#include "IBLPrecompute.h"
#include "ResourceManager.h"
#include <cmath>
#include <stdexcept>

namespace
{
    void checkCaptureComplete()
    {
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            throw std::runtime_error("Incomplete IBL capture framebuffer");
    }

    struct CaptureTarget
    {
        GLuint fbo = 0, rbo = 0;
        CaptureTarget() { glGenFramebuffers(1, &fbo); glGenRenderbuffers(1, &rbo); }
        ~CaptureTarget() { glDeleteFramebuffers(1, &fbo); glDeleteRenderbuffers(1, &rbo); }
    };

    // Baking is a self-contained operation, including on exception paths.
    struct BakeState
    {
        GLint drawFbo, readFbo, rbo, program, vao, activeTexture, texture2D, textureCube;
        GLint viewport[4], polygonMode[2], depthFunc;
        GLboolean cull, depth, blend, scissor, srgb, discard, depthMask, colorMask[4];
        GLdouble clearDepth;
        BakeState()
        {
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFbo);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFbo);
            glGetIntegerv(GL_RENDERBUFFER_BINDING, &rbo);
            glGetIntegerv(GL_CURRENT_PROGRAM, &program);
            glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
            glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
            glActiveTexture(GL_TEXTURE0);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2D);
            glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &textureCube);
            glGetIntegerv(GL_VIEWPORT, viewport);
            glGetIntegerv(GL_POLYGON_MODE, polygonMode);
            glGetIntegerv(GL_DEPTH_FUNC, &depthFunc);
            glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
            glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
            glGetDoublev(GL_DEPTH_CLEAR_VALUE, &clearDepth);
            cull = glIsEnabled(GL_CULL_FACE);
            depth = glIsEnabled(GL_DEPTH_TEST);
            blend = glIsEnabled(GL_BLEND);
            scissor = glIsEnabled(GL_SCISSOR_TEST);
            srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
            discard = glIsEnabled(GL_RASTERIZER_DISCARD);
        }
        static void restore(GLenum cap, GLboolean value) { if (value) glEnable(cap); else glDisable(cap); }
        ~BakeState()
        {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFbo);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, readFbo);
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glUseProgram(program);
            glBindVertexArray(vao);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture2D);
            glBindTexture(GL_TEXTURE_CUBE_MAP, textureCube);
            glActiveTexture(activeTexture);
            glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
            glPolygonMode(GL_FRONT_AND_BACK, polygonMode[0]);
            glDepthFunc(depthFunc);
            glDepthMask(depthMask);
            glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
            glClearDepth(clearDepth);
            restore(GL_CULL_FACE, cull);
            restore(GL_DEPTH_TEST, depth);
            restore(GL_BLEND, blend);
            restore(GL_SCISSOR_TEST, scissor);
            restore(GL_FRAMEBUFFER_SRGB, srgb);
            restore(GL_RASTERIZER_DISCARD, discard);
        }
    };
}

EnvironmentGpuResources::~EnvironmentGpuResources()
{
    if (envCubemap) glDeleteTextures(1, &envCubemap);
    if (irradianceMap) glDeleteTextures(1, &irradianceMap);
    if (prefilterMap) glDeleteTextures(1, &prefilterMap);
}

EnvironmentGpuView EnvironmentGpuResources::view() const
{
    return {envCubemap, irradianceMap, prefilterMap, brdfLUT ? brdfLUT->getID() : 0};
}

void IBLPrecompute::initialize(ResourceManager& resources)
{
    skyboxShader = resources.GetShader("skybox");
    irradianceShader = resources.GetShader("irradiance");
    prefilterShader = resources.GetShader("prefilter");
    brdfShader = resources.GetShader("brdf");
    cube = resources.GetCube();
    screenQuad = resources.GetScreenQuad();
}

void IBLPrecompute::shutdown()
{
    cachedBrdfLUT.reset();
    cachedBrdfProgram = 0;
    cube.reset();
    screenQuad.reset();
    skyboxShader.reset();
    irradianceShader.reset();
    prefilterShader.reset();
    brdfShader.reset();
}

std::array<GLuint, 4> IBLPrecompute::programIds() const
{
    return {skyboxShader ? skyboxShader->ID : 0, irradianceShader ? irradianceShader->ID : 0,
        prefilterShader ? prefilterShader->ID : 0, brdfShader ? brdfShader->ID : 0};
}

std::unique_ptr<EnvironmentGpuResources> IBLPrecompute::bake(GLuint hdrTexture)
{
    if (hdrTexture == 0 || !glIsTexture(hdrTexture))
        throw std::invalid_argument("IBL requires a valid HDR texture");
    if (!cube || !screenQuad) throw std::logic_error("IBLPrecompute is not initialized");
    for (GLuint program : programIds())
    {
        GLint linked = GL_FALSE;
        if (program) glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) throw std::runtime_error("IBL shader is missing or not linked");
    }
    const auto resolution = settings.environmentSize;
    const auto maxMipLevels = settings.prefilterMipLevels;
    if (!resolution || !settings.irradianceSize || !settings.prefilterSize || !settings.brdfSize ||
        !maxMipLevels || maxMipLevels > 1u + static_cast<unsigned>(std::log2(settings.prefilterSize)))
        throw std::invalid_argument("Invalid IBL sizes/mip count");

    BakeState state;
    CaptureTarget capture;
    const auto captureFBO = capture.fbo;
    const auto captureRBO = capture.rbo;
    auto result = std::make_unique<EnvironmentGpuResources>();
    auto& maps = *result;
    const GLuint envMap = hdrTexture;
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearDepth(1.0);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_RASTERIZER_DISCARD);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    glGenTextures(1, &maps.envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, maps.envCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, resolution, resolution, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    std::array<glm::mat4, 6> projViewMatrix = ResourceManager::calculateCubeMatrices(glm::vec3(0.0f));
    skyboxShader->use();
    skyboxShader->setUniform("equirectangularMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, envMap);

    glViewport(0, 0, resolution, resolution);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glDisable(GL_CULL_FACE);
    for (unsigned int i = 0; i < 6; ++i)
    {
        skyboxShader->setUniform("projView", projViewMatrix[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, maps.envCubemap, 0);
        checkCaptureComplete();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube->draw();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindTexture(GL_TEXTURE_CUBE_MAP, maps.envCubemap);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
    // --------------------------------------------------------------------------------
    glGenTextures(1, &maps.irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, maps.irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, settings.irradianceSize, settings.irradianceSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, settings.irradianceSize, settings.irradianceSize);

    // pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
    // -----------------------------------------------------------------------------
    irradianceShader->use();
    irradianceShader->setUniform("environmentMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, maps.envCubemap);

    glViewport(0, 0, settings.irradianceSize, settings.irradianceSize); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        irradianceShader->setUniform("projView", projViewMatrix[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, maps.irradianceMap, 0);
        checkCaptureComplete();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube->draw();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // pbr: create a pre-filter cubemap, and re-scale capture FBO to pre-filter scale.
    // --------------------------------------------------------------------------------
    glGenTextures(1, &maps.prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, maps.prefilterMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, settings.prefilterSize, settings.prefilterSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minification filter to mip_linear
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, maxMipLevels - 1);
    // generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // pbr: run a quasi monte-carlo simulation on the environment lighting to create a prefilter (cube)map.
    // ----------------------------------------------------------------------------------------------------
    prefilterShader->use();
    prefilterShader->setUniform("environmentMap", 0);
    prefilterShader->setUniform("environmentResolution", static_cast<float>(resolution));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, maps.envCubemap);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
    {
        // reisze framebuffer according to mip-level size.
        unsigned int mipWidth = static_cast<unsigned int>(settings.prefilterSize >> mip);
        unsigned int mipHeight = static_cast<unsigned int>(settings.prefilterSize >> mip);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = maxMipLevels > 1 ? (float)mip / (float)(maxMipLevels - 1) : 0.0f;
        prefilterShader->setUniform("roughness", roughness);
        for (unsigned int i = 0; i < 6; ++i)
        {
            prefilterShader->setUniform("projView", projViewMatrix[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, maps.prefilterMap, mip);
            checkCaptureComplete();

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            cube->draw();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    const bool rebuildLut = !cachedBrdfLUT || cachedBrdfProgram != brdfShader->ID;
    if (rebuildLut)
    {
        // pbr: generate a 2D LUT from the BRDF equations used.
        // ----------------------------------------------------
        GLuint lutId = 0;
        glGenTextures(1, &lutId);
        Texture newLut(lutId);
        maps.brdfLUT = std::make_shared<Texture>(std::move(newLut));

        // pre-allocate enough memory for the LUT texture.
        glBindTexture(GL_TEXTURE_2D, lutId);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, settings.brdfSize, settings.brdfSize, 0, GL_RG, GL_FLOAT, 0);
        // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
        glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, settings.brdfSize, settings.brdfSize);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lutId, 0);

        checkCaptureComplete();
        glViewport(0, 0, settings.brdfSize, settings.brdfSize);
        brdfShader->use();
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        screenQuad->draw();
    }
    else maps.brdfLUT = cachedBrdfLUT;

    if (glGetError() != GL_NO_ERROR)
        throw std::runtime_error("OpenGL error during IBL allocation/bake");
    if (rebuildLut)
    {
        cachedBrdfLUT = maps.brdfLUT;
        cachedBrdfProgram = brdfShader->ID;
    }
    return result;
}
