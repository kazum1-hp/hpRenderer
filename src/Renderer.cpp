#include "../include/Renderer.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtc/matrix_access.hpp>
#include "../include/ResourceManager.h"
#include "../include/Scene.h"
#include <cstdio> // for std::snprintf

static inline void SafeCopyPath(char* dest, size_t destSize, const std::string& src)
{
    if (dest == nullptr || destSize == 0) return;
    // Use snprintf to perform a safe copy and ensure null-terminate
    std::snprintf(dest, destSize, "%s", src.c_str());
    dest[destSize - 1] = '\0';
}

Renderer::Renderer(Camera& cam, InputManager& input, Window& win, Scene& scene)
    :camera(cam),
     input(input),
     window(win),
     scene(scene)
{
    if (!window.getWindow()) {
        std::cerr << "Renderer initialization skipped: no valid GLFW window." << std::endl;
        return;
    }

    if (!glfwGetCurrentContext()) {
        glfwMakeContextCurrent(window.getWindow());
    }

    if (!glfwGetCurrentContext()) {
        std::cerr << "Renderer initialization skipped: no current OpenGL context." << std::endl;
        return;
    }

    framebufferWidth = window.getWidth();
    framebufferHeight = window.getHeight();
    framebuffers.push_back(std::make_unique<FrameBuffer>(window, /*useDepth*/true, /*useMs*/false, /*useDepthMap2D*/false, /*useDepthCube*/false, /*useHdr*/useHdr, 2));
    framebuffers.push_back(std::make_unique<FrameBuffer>(SHADOW_Size, SHADOW_Size, false, false, true, false, false));
    framebuffers.push_back(std::make_unique<FrameBuffer>(window, /*useDepth*/false, /*useMs*/false, /*useDepthMap2D*/false, /*useDepthCube*/false, /*useHdr*/true, 5, /*useGbuffer*/true));
    framebuffers.push_back(std::make_unique<FrameBuffer>(window, /*useDepth*/true, /*useMs*/false, /*useDepthMap2D*/false, /*useDepthCube*/false, /*useHdr*/useHdr, 2));
    framebuffers.push_back(std::make_unique<FrameBuffer>(window, /*useDepth*/true, /*useMs*/false, /*useDepthMap2D*/false, /*useDepthCube*/false, /*useHdr*/false));

    pingpongFrameBuffer[0] = std::make_unique<FrameBuffer>(window, false, false, false, false, true);
    pingpongFrameBuffer[1] = std::make_unique<FrameBuffer>(window, false, false, false, false, true);

    window.onFramebufferResize = [this](int w, int h) {
        glViewport(0, 0, w, h);
        };

    // --- glEnable ---
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    /*glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);*/
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

void Renderer::init()
{
    InitConsole();

    // 1. Get ResourceManager instance
    auto& res = ResourceManager::GetInstance();

    // 2. Load pipeline core 
    // 
    // 
    // 
    // Assuming these shaders have already been preloaded into the manager by external or GameInit, or are being loaded for the first time here.
    modelShader = res.GetShader("model");
    lightShader = res.GetShader("light");
    sceneFramebufferShader = res.GetShader("scene framebuffer");
    skyboxShader = res.GetShader("skybox");
    dirShadowShader = res.GetShader("dir shadow");
    pointShadowShader = res.GetShader("point shadow");
    bloomBlurShader = res.GetShader("bloomBlur");
    gBufferShader = res.GetShader("gBuffer");;
    lightPassShader = res.GetShader("lightPass");
    debugShader = res.GetShader("debug");
    gbufferDebugShader = res.GetShader("gbuffer debug");
    backgroundShader = res.GetShader("background");
    irradianceShader = res.GetShader("irradiance");
    prefilterShader = res.GetShader("prefilter");
    brdfShader = res.GetShader("brdf");

    // Get fullscreen Quad 
    screenQuad = res.GetScreenQuad();
    plane = res.GetPlane();
    cube = res.GetCube();

    for (int i = 0; i < scene.GetPointLights().size(); i++)
    {
        pointShadowFramebuffers.push_back(
            std::make_unique<FrameBuffer>(SHADOW_Size, SHADOW_Size,
                false, false,
                false,  // useDepthMap2D
                true,   // useDepthCube
                false)  // useHdr
        );
    }

    modelShader->use();
    modelShader->setUniform("depthMap", 0);
    for (int i = 0; i < 4; ++i)
    {
        modelShader->setUniform("shadowMap[" + std::to_string(i) + "]", 1 + i);
    }
    modelShader->setUniform("diffuse", 5);
    modelShader->setUniform("specular", 6);
    modelShader->setUniform("normal", 7);
    modelShader->setUniform("height", 8);
    modelShader->setUniform("arm", 9);
    modelShader->setUniform("irradianceMap", 11);
    modelShader->setUniform("prefilterMap", 12);
    modelShader->setUniform("brdfLUT", 13);

    for (int i = 0; i < scene.GetPointLights().size(); i++)
    {
        std::string base = "pointLight[" + std::to_string(i) + "]";

        modelShader->setUniform(base + ".constant", 1.0f);
        modelShader->setUniform(base + ".linear", 0.09f);
        modelShader->setUniform(base + ".quadratic", 0.032f);
    }

    bloomBlurShader->use();
    bloomBlurShader->setUniform("image", 0);

    backgroundShader->use();
    backgroundShader->setUniform("environmentMap", 0);  

    // defered rendering lightPass
    lightPassShader->use();
    lightPassShader->setUniform("gPosition", 0);
    lightPassShader->setUniform("gNormal", 1);
    lightPassShader->setUniform("gAlbedo", 2);
    lightPassShader->setUniform("gARM", 3); // ao,roughness,metallic packed
    lightPassShader->setUniform("gGeoNormal", 4);
    lightPassShader->setUniform("gDepth", 5);
    lightPassShader->setUniform("depthMap", 6);
    for (int i = 0; i < 4; ++i)
    {
        lightPassShader->setUniform("shadowMap[" + std::to_string(i) + "]", 7 + i);
    }

    // IBL samplers
    lightPassShader->setUniform("irradianceMap", 11);
    lightPassShader->setUniform("prefilterMap", 12);
    lightPassShader->setUniform("brdfLUT", 13);

    for (int i = 0; i < scene.GetPointLights().size(); i++)
    {
        std::string base = "pointLight[" + std::to_string(i) + "]";

        lightPassShader->setUniform(base + ".constant", 1.0f);
        lightPassShader->setUniform(base + ".linear", 0.09f);
        lightPassShader->setUniform(base + ".quadratic", 0.032f);
    }
}

void Renderer::prepareEnvironment(Environment& env)
{
    if (!env.asset) return;

    if (env.maps.isGenerated &&
        env.maps.lastHDR == env.asset->hdrTexture)
        return;

    generateIBLMaps(env);

    env.maps.lastHDR = env.asset->hdrTexture;
    env.maps.isGenerated = true;
}

void Renderer::generateIBLMaps(Environment& env)
{
    auto& res = ResourceManager::GetInstance();

    GLuint envMap = env.asset->hdrTexture;

    unsigned int captureFBO;
    unsigned int captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, resolution, resolution);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);

    glGenTextures(1, &env.maps.envCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.envCubemap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, resolution, resolution, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    std::array<glm::mat4, 6> projViewMatrix = res.calculateCubeMatrices(glm::vec3(0.0f));
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
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, env.maps.envCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube->draw();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // pbr: create an irradiance cubemap, and re-scale capture FBO to irradiance scale.
    // --------------------------------------------------------------------------------
    glGenTextures(1, &env.maps.irradianceMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.irradianceMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 32, 32, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 32, 32);

    // pbr: solve diffuse integral by convolution to create an irradiance (cube)map.
    // -----------------------------------------------------------------------------
    irradianceShader->use();
    irradianceShader->setUniform("environmentMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.envCubemap);

    glViewport(0, 0, 32, 32); // don't forget to configure the viewport to the capture dimensions.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    for (unsigned int i = 0; i < 6; ++i)
    {
        irradianceShader->setUniform("projView", projViewMatrix[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, env.maps.irradianceMap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        cube->draw();
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // pbr: create a pre-filter cubemap, and re-scale capture FBO to pre-filter scale.
    // --------------------------------------------------------------------------------
    glGenTextures(1, &env.maps.prefilterMap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.prefilterMap);
    for (unsigned int i = 0; i < 6; ++i)
    {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, 128, 128, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR); // be sure to set minification filter to mip_linear 
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // generate mipmaps for the cubemap so OpenGL automatically allocates the required memory.
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

    // pbr: run a quasi monte-carlo simulation on the environment lighting to create a prefilter (cube)map.
    // ----------------------------------------------------------------------------------------------------
    prefilterShader->use();
    prefilterShader->setUniform("environmentMap", 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.envCubemap);

    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    unsigned int maxMipLevels = 5;
    for (unsigned int mip = 0; mip < maxMipLevels; ++mip)
    {
        // reisze framebuffer according to mip-level size.
        unsigned int mipWidth = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        unsigned int mipHeight = static_cast<unsigned int>(128 * std::pow(0.5, mip));
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, mipWidth, mipHeight);
        glViewport(0, 0, mipWidth, mipHeight);

        float roughness = (float)mip / (float)(maxMipLevels - 1);
        prefilterShader->setUniform("roughness", roughness);
        for (unsigned int i = 0; i < 6; ++i)
        {
            prefilterShader->setUniform("projView", projViewMatrix[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, env.maps.prefilterMap, mip);

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            cube->draw();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // pbr: generate a 2D LUT from the BRDF equations used.
    // ----------------------------------------------------
    glGenTextures(1, &env.maps.brdfLUT);

    // pre-allocate enough memory for the LUT texture.
    glBindTexture(GL_TEXTURE_2D, env.maps.brdfLUT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, 512, 512, 0, GL_RG, GL_FLOAT, 0);
    // be sure to set wrapping mode to GL_CLAMP_TO_EDGE
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // then re-configure capture framebuffer object and render screen-space quad with BRDF shader.
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 512, 512);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, env.maps.brdfLUT, 0);

    glViewport(0, 0, 512, 512);
    brdfShader->use();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    screenQuad->draw();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, framebufferWidth, framebufferHeight);
}

void Renderer::render(Scene& scene)
{
    prepareEnvironment(scene.GetEnvironment());
    
    LightSpaceMatrix = scene.GetDirLight().getOrthoMatrix() * scene.GetDirLight().getOrthoViewMatrix();

    if (useShadows)
        shadowPass(scene);
    else
    {
        parallelShadows = false;
        pointShadows = false;
    }

    FrameBuffer& hdrFrameBuffer = *framebuffers[0];
    FrameBuffer& lightPassFrameBuffer = *framebuffers[3];
    FrameBuffer& postProcessFrameBuffer = *framebuffers[4];

    if (!useDeferred)
    {        
        if (usePostProcess)
        {
            forwardPass(scene);
            postProcessPass(hdrFrameBuffer);
            finalTexture = postProcessFrameBuffer.getColor();
        }

        else
        {
            forwardPass(scene);
            finalTexture = hdrFrameBuffer.getColor();
        }
    }
    else
    {
        deferredPass(scene);
        postProcessPass(lightPassFrameBuffer);
        finalTexture = postProcessFrameBuffer.getColor();
    }
}

void Renderer::shadowPass(Scene& scene)
{
    //glm::mat4 LightSpaceMatrix = dirLight.getOrthoMatrix() * dirLight.getOrthoViewMatrix();
    glm::mat4 model(1.0f);

    FrameBuffer& parallelShadowFrameBuffer = *framebuffers[1];

    glViewport(0, 0, SHADOW_Size, SHADOW_Size);

    if (input.isParallelLightOn())
    {
        glBindFramebuffer(GL_FRAMEBUFFER, parallelShadowFrameBuffer.getFBO());
        glClear(GL_DEPTH_BUFFER_BIT);

        dirShadowShader->use();

        dirShadowShader->setUniform("lightSpaceMatrix", LightSpaceMatrix);
        dirShadowShader->setUniform("model", model);
        glDisable(GL_CULL_FACE);
        plane->draw();
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        for (const auto& obj : scene.GetObjects()) {
            renderModel(obj.transform, *obj.model, *dirShadowShader);
        }
        glCullFace(GL_BACK);
    }

    if (input.isPointLightOn())
    {
        pointShadowShader->use();

        for (int i = 0; i < scene.GetPointLights().size(); i++)
        {
            FrameBuffer& fbo = *pointShadowFramebuffers[i];

            glBindFramebuffer(GL_FRAMEBUFFER, fbo.getFBO());
            glClear(GL_DEPTH_BUFFER_BIT);

            for (GLuint j = 0; j < 6; ++j)
            {
                pointShadowShader->setUniform("shadowMatrices[" + std::to_string(j) + "]", scene.GetPointLights()[i].getPerspTransMatrix(j));
            }

            pointShadowShader->setUniform("far_plane", scene.GetPointLights()[i].getFar());
            pointShadowShader->setUniform("lightPos", scene.GetPointLights()[i].getLightPos());

            pointShadowShader->setUniform("model", model);
            glDisable(GL_CULL_FACE);
            plane->draw();
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            for (const auto& obj : scene.GetObjects()) {
                renderModel(obj.transform, *obj.model, *pointShadowShader);
            }
            glCullFace(GL_BACK);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, framebufferWidth, framebufferHeight);
}

void Renderer::forwardPass(Scene& scene)
{
    glm::mat4 model(1.0f);

    FrameBuffer& parallelShadowFrameBuffer = *framebuffers[1];

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[0]->getFBO());
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, framebufferWidth, framebufferHeight); 
    // object
    modelShader->use();

    modelShader->setUniform("usePost", usePostProcess);

    modelShader->setUniform("aoBias", aoBias);
    modelShader->setUniform("roughnessBias", roughnessBias);
    modelShader->setUniform("metallicBias", metallicBias);

    // transform matrix
    modelShader->setUniform("view", camera.getViewMatrix());
    modelShader->setUniform("projection", camera.getProjectionMatrix());
    modelShader->setUniform("viewPos", camera.getPosition());

    // light 
    //modelShader->setUniform("useBlinnPhong", useBlinnPhong);
    modelShader->setUniform("useQuadratic", useQuadratic);
    //modelShader->setUniform("modelLight", modelLight);

    // paralleLight
    modelShader->setUniform("parallelLight.color", scene.GetDirLight().getColor());
    modelShader->setUniform("parallelLight.direction", scene.GetDirLight().getLightDir());
    modelShader->setUniform("parallelLight.intensity", scene.GetDirLight().getIntensity());
    modelShader->setUniform("parallelLight.enabled", input.isParallelLightOn());
    modelShader->setUniform("lightSpaceMatrix", LightSpaceMatrix);
    modelShader->setUniform("parallelShadows", parallelShadows);
    modelShader->setUniform("pointShadows", pointShadows);

    if (useShadows)
    {
        if (input.isParallelLightOn())
        {
            pointShadows = false;
            parallelShadows = true;
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, parallelShadowFrameBuffer.getDepth2D());

            modelShader->setUniform("depthMap", 0);
        }
        else pointShadows = true;
    }

    //point light

    for (int i = 0; i < scene.GetPointLights().size(); i++)
    {
        std::string base = "pointLight[" + std::to_string(i) + "]";

        modelShader->setUniform(base + ".color", scene.GetPointLights()[i].getColor());
        modelShader->setUniform(base + ".position", scene.GetPointLights()[i].getLightPos());
        modelShader->setUniform(base + ".intensity", scene.GetPointLights()[i].getIntensity());
        modelShader->setUniform(base + ".enabled", scene.GetPointLights()[i].lightOn() && input.isPointLightOn());
        modelShader->setUniform("far_plane", scene.GetPointLights()[i].getFar());

        if (input.isPointLightOn() && useShadows)
        {
            glActiveTexture(GL_TEXTURE1 + i);
            glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowFramebuffers[i]->getDepthCube());

            modelShader->setUniform("shadowMap[" + std::to_string(i) + "]", 1 + i);
        }
    }

    if (drawPlane)
    {
        drawMesh(*plane, *modelShader);
        modelShader->setUniform("model", model);
        modelShader->setUniform("hasNormalMap", false);
        modelShader->setUniform("hasHeightMap", false);
        modelShader->setUniform("hasARMMap", false);
        glDisable(GL_CULL_FACE);
        plane->draw();
        glEnable(GL_CULL_FACE);
    }

    auto& env = scene.GetEnvironment();
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.irradianceMap);
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.prefilterMap);
    glActiveTexture(GL_TEXTURE13);
    glBindTexture(GL_TEXTURE_2D, env.maps.brdfLUT);

    for (const auto& obj : scene.GetObjects()) {
        drawModel(*obj.model, *modelShader);
        modelShader->setUniform("hasNormalMap", hasNormal);
        modelShader->setUniform("hasHeightMap", hasHeight);
        modelShader->setUniform("height_scale", height_scale);
        modelShader->setUniform("hasARMMap", true);
        renderModel(obj.transform, *obj.model, *modelShader);
    }

    if (drawLights)
    {
        // draw cube lights
        lightShader->use();

        ////// transform matrix
        lightShader->setUniform("view", camera.getViewMatrix());
        lightShader->setUniform("projection", camera.getProjectionMatrix());

        for (int i = 0; i < scene.GetPointLights().size(); i++)
        {
            glm::mat4 lightModel(1.0f);
            lightModel = glm::translate(model, scene.GetPointLights()[i].getLightPos());
            lightModel = glm::scale(lightModel, glm::vec3(0.25f));
            lightShader->setUniform("model", lightModel);
            lightShader->setUniform("lightColor", scene.GetPointLights()[i].getColor());
            lightShader->setUniform("enabled", scene.GetPointLights()[i].lightOn() && input.isPointLightOn());
            if (scene.GetPointLights()[i].lightOn() && input.isPointLightOn())
            {
                cube->draw();
            }
        }
    }

    glDisable(GL_CULL_FACE); // or glCullFace(GL_FRONT)

    backgroundShader->use();
    backgroundShader->setUniform("view", camera.getViewMatrix());
    backgroundShader->setUniform("projection", camera.getProjectionMatrix());
    backgroundShader->setUniform("usePost", usePostProcess);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.envCubemap);
    cube->draw();

    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

void Renderer::postProcessPass(const FrameBuffer& framebuffer)
{
    // bloom
    //FrameBuffer& hdrFrameBuffer = *framebuffers[0];

    glDisable(GL_DEPTH_TEST);

    bool horizontal = true, first_iteration = true;
    unsigned int amount = 5;

    if (useBloom)
    {   
        bloomBlurShader->use();
        bloomBlurShader->setUniform("samplerDistance", samplerDistance);

        for (unsigned int i = 0; i < amount; i++)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, pingpongFrameBuffer[horizontal]->getFBO());
            bloomBlurShader->setUniform("horizontal", horizontal);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, first_iteration ? framebuffer.getColor(1) : pingpongFrameBuffer[!horizontal]->getColor());  // bind texture of other framebuffer (or scene if first iteration)
            screenQuad->draw();
            horizontal = !horizontal;
            if (first_iteration)
                first_iteration = false;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, framebuffers[4]->getFBO());
    glViewport(0, 0, framebufferWidth, framebufferHeight);

    sceneFramebufferShader->use();

    sceneFramebufferShader->setUniform("effectMode", effectMode);
    sceneFramebufferShader->setUniform("offset", offset);
    sceneFramebufferShader->setUniform("screenTexture", 0);
    sceneFramebufferShader->setUniform("blur", 1);
    sceneFramebufferShader->setUniform("scanPos", scanPos);
    sceneFramebufferShader->setUniform("useGamma", useGamma);
    sceneFramebufferShader->setUniform("useHdr", useHdr);
    sceneFramebufferShader->setUniform("useBloom", useBloom);
    sceneFramebufferShader->setUniform("exposure", exposure);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, framebuffer.getColor(0));

    if (useBloom)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, pingpongFrameBuffer[!horizontal]->getColor());
    }

    screenQuad->draw();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void Renderer::deferredPass(Scene& scene)
{
    geometryPass(scene);
    lightPass(scene);
}

void Renderer::geometryPass(Scene& scene)
{
    glm::mat4 model(1.0f);

    FrameBuffer& gFrameBuffer = *framebuffers[2];
    GLuint gBuffer = gFrameBuffer.getFBO();

    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gBufferShader->use();
    // uniform setting
    gBufferShader->setUniform("view", camera.getViewMatrix());
    gBufferShader->setUniform("projection", camera.getProjectionMatrix());
    gBufferShader->setUniform("viewPos", camera.getPosition());

    // PBR / ORM settings
    gBufferShader->setUniform("aoBias", aoBias);
    gBufferShader->setUniform("roughnessBias", roughnessBias);
    gBufferShader->setUniform("metallicBias", metallicBias);

    // Per-mesh flags handled in drawModel/drawMesh, but set global toggles here
    gBufferShader->setUniform("hasNormalMap", hasNormal);
    gBufferShader->setUniform("hasHeightMap", hasHeight);
    gBufferShader->setUniform("hasARMMap", true); // assume models may provide ARM map; drawMesh will bind when present
    gBufferShader->setUniform("height_scale", height_scale);

    if (drawPlane)
    {
        drawMesh(*plane, *gBufferShader);
        gBufferShader->setUniform("model", model);
        gBufferShader->setUniform("hasNormalMap", false);
        gBufferShader->setUniform("hasHeightMap", false);
        gBufferShader->setUniform("hasARMMap", false);
        glDisable(GL_CULL_FACE);
        plane->draw();
        glEnable(GL_CULL_FACE);
    }

    for (const auto& obj : scene.GetObjects()) {
        drawModel(*obj.model, *gBufferShader);
        gBufferShader->setUniform("hasNormalMap", hasNormal);
        gBufferShader->setUniform("hasHeightMap", hasHeight);
        gBufferShader->setUniform("height_scale", height_scale);
        gBufferShader->setUniform("hasARMMap", true);
        renderModel(obj.transform, *obj.model, *gBufferShader);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::lightPass(Scene& scene)
{
    glm::mat4 model(1.0f);

    FrameBuffer& parallelShadowFrameBuffer = *framebuffers[1];
    FrameBuffer& gFrameBuffer = *framebuffers[2];
    FrameBuffer& lightPassFrameBuffer = *framebuffers[3];

    GLuint gBuffer = gFrameBuffer.getFBO();
    GLuint gPosition = gFrameBuffer.getColor(0);
    GLuint gNormal = gFrameBuffer.getColor(1);
    GLuint gAlbedo = gFrameBuffer.getColor(2);
    GLuint gARM = gFrameBuffer.getColor(3);
    GLuint gGeoNormal = gFrameBuffer.getColor(4);
    GLuint gDepth = gFrameBuffer.getGDepth();

    glBindFramebuffer(GL_FRAMEBUFFER, lightPassFrameBuffer.getFBO());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // lightPass.use()
    //Shader& lightPassShader = *shaders[8];
    lightPassShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gPosition);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gNormal);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gAlbedo);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, gARM);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, gGeoNormal);
    glActiveTexture(GL_TEXTURE5);
    glBindTexture(GL_TEXTURE_2D, gDepth);

    // uniform settings
    lightPassShader->setUniform("viewPos", camera.getPosition());

    // light 
    //lightPassShader->setUniform("useBlinnPhong", useBlinnPhong);
    lightPassShader->setUniform("useQuadratic", useQuadratic);
    //lightPassShader->setUniform("modelLight", modelLight);

    // paralleLight
    lightPassShader->setUniform("parallelLight.color", scene.GetDirLight().getColor());
    lightPassShader->setUniform("parallelLight.direction", scene.GetDirLight().getLightDir());
    lightPassShader->setUniform("parallelLight.intensity", scene.GetDirLight().getIntensity());
    lightPassShader->setUniform("parallelLight.enabled", input.isParallelLightOn());
    lightPassShader->setUniform("lightSpaceMatrix", LightSpaceMatrix);
    lightPassShader->setUniform("parallelShadows", parallelShadows);
    lightPassShader->setUniform("pointShadows", pointShadows);

    if (useShadows)
    {
        if (input.isParallelLightOn())
        {
            pointShadows = false;
            parallelShadows = true;
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, parallelShadowFrameBuffer.getDepth2D());

            lightPassShader->setUniform("depthMap", 6);
        }
        else pointShadows = true;
    }

    //point light

    for (int i = 0; i < scene.GetPointLights().size(); i++)
    {
        std::string base = "pointLight[" + std::to_string(i) + "]";

        lightPassShader->setUniform(base + ".color", scene.GetPointLights()[i].getColor());
        lightPassShader->setUniform(base + ".position", scene.GetPointLights()[i].getLightPos());
        lightPassShader->setUniform(base + ".intensity", scene.GetPointLights()[i].getIntensity());
        lightPassShader->setUniform(base + ".enabled", scene.GetPointLights()[i].lightOn() && input.isPointLightOn());
        lightPassShader->setUniform("far_plane", scene.GetPointLights()[i].getFar());

        if (input.isPointLightOn() && useShadows)
        {
            glActiveTexture(GL_TEXTURE7 + i);
            glBindTexture(GL_TEXTURE_CUBE_MAP, pointShadowFramebuffers[i]->getDepthCube());

            lightPassShader->setUniform("shadowMap[" + std::to_string(i) + "]", 7 + i);
        }
    }

    // bind IBL environment maps (same units as modelShader)
    auto& env = scene.GetEnvironment();
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.irradianceMap);
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.prefilterMap);
    glActiveTexture(GL_TEXTURE13);
    glBindTexture(GL_TEXTURE_2D, env.maps.brdfLUT);

    // screen quad draw
    screenQuad->draw();
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, lightPassFrameBuffer.getFBO());
    glBlitFramebuffer(0, 0, framebufferWidth, framebufferHeight,
                      0, 0, framebufferWidth, framebufferHeight,
                      GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    //glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glBindFramebuffer(GL_FRAMEBUFFER, lightPassFrameBuffer.getFBO());
    //glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // lightCube draw
    if (drawLights)
    {
        lightShader->use();

        // transform matrix
        lightShader->setUniform("view", camera.getViewMatrix());
        lightShader->setUniform("projection", camera.getProjectionMatrix());

        for (int i = 0; i < scene.GetPointLights().size(); i++)
        {
            glm::mat4 lightModel(1.0f);
            lightModel = glm::translate(model, scene.GetPointLights()[i].getLightPos());
            lightModel = glm::scale(lightModel, glm::vec3(0.25f));
            lightShader->setUniform("model", lightModel);
            lightShader->setUniform("lightColor", scene.GetPointLights()[i].getColor());
            lightShader->setUniform("enabled", scene.GetPointLights()[i].lightOn() && input.isPointLightOn());
            if (scene.GetPointLights()[i].lightOn() && input.isPointLightOn())
            {
                cube->draw();
            }
        }
    }

    glDisable(GL_CULL_FACE); // or glCullFace(GL_FRONT)

    backgroundShader->use();
    backgroundShader->setUniform("view", camera.getViewMatrix());
    backgroundShader->setUniform("projection", camera.getProjectionMatrix());
    backgroundShader->setUniform("usePost", true);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.maps.envCubemap);
    cube->draw();

    glEnable(GL_CULL_FACE);

    glDisable(GL_DEPTH_TEST);

    if (drawDebug)
    {
        int debugH = framebufferHeight / 4;
        int debugW = debugH * static_cast<int>(camera.aspect);

        gbufferDebugShader->use();
        glBindVertexArray(screenQuad->getVAO());

        auto drawDebug = [&](GLuint tex, int index)
            {
                glViewport(
                    0,                  // x
                    framebufferHeight - (index + 1) * debugH,  // OpenGL origin is at the lower left.
                    debugW,
                    debugH
                );

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex);
                gbufferDebugShader->setUniform("debugTex", 0);
                gbufferDebugShader->setUniform("u_DebugMode", index);
                gbufferDebugShader->setUniform("fov", camera.getFov());
                gbufferDebugShader->setUniform("nearPlane", camera.getNearPlane());
                gbufferDebugShader->setUniform("farPlane", camera.getFarPlane());
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            };

        drawDebug(gNormal, 0);
        drawDebug(gARM, 1);
        drawDebug(gARM, 2);
        drawDebug(gDepth, 3);
    }

    glViewport(0, 0, framebufferWidth, framebufferHeight);
    glEnable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::drawMesh(const Mesh& mesh, Shader& shader) const
{
    for (const auto& tex : mesh.getTexture())
    {
        GLuint slot = 0;
        std::string uniformName;

        switch (tex->getType())
        {
            case TextureType::Diffuse:
                uniformName = "diffuse";
                slot = 5;
                break;
            case TextureType::Specular:
                uniformName = "specular";
                slot = 6;
                break;
            case TextureType::Normal:
                uniformName = "normal";
                slot = 7;
                break;
            case TextureType::Height:
                uniformName = "height";
                slot = 8;
                break;
            case TextureType::ARM:
                uniformName = "arm";
                slot = 9;
                break;
            default:
                continue;
        }

        tex->bind(slot);
        shader.setUniform(uniformName, static_cast<int>(slot));
    }
}

void Renderer::drawModel(const Model& model, Shader& shader) const
{
    for (auto& mesh : model.meshes)
    {
        drawMesh(*mesh, shader);
    }
}

void Renderer::renderModel(const Transform& transform, const Model& model, Shader& shader)
{
    glm::mat4 modelMatrix = transform.getModelMatrix();

    shader.setUniform("model", modelMatrix);

    model.draw();
}

void Renderer::resizeFrameBuffer(unsigned int newWidth, unsigned int newHeight)
{
    FrameBuffer& hdrFrameBuffer = *framebuffers[0];
    FrameBuffer& gFrameBuffer = *framebuffers[2];
    FrameBuffer& lightPassFrameBuffer = *framebuffers[3];
    FrameBuffer& postProcessFrameBuffer = *framebuffers[4];

    hdrFrameBuffer.resize(newWidth, newHeight);
    gFrameBuffer.resize(newWidth, newHeight);
    lightPassFrameBuffer.resize(newWidth, newHeight);
    postProcessFrameBuffer.resize(newWidth, newHeight);
    pingpongFrameBuffer[0]->resize(newWidth, newHeight);
    pingpongFrameBuffer[1]->resize(newWidth, newHeight);

    camera.aspect = (float)newWidth / (float)newHeight;

    framebufferWidth = newWidth;
    framebufferHeight = newHeight;
}

std::stringstream Renderer::buffer;

void Renderer::InitConsole()
{
    static bool initialized = false;
    if (!initialized)
    {
        std::cout.rdbuf(buffer.rdbuf());
        std::cerr.rdbuf(buffer.rdbuf());
        initialized = true;
    }
}

void Renderer::onImGuiRender()
{
    // --- DockSpace / Layout setup (full-screen) ---
    ImGuiIO& io = ImGui::GetIO();

    ImGuiID left_bottom_id = 0;
    ImGuiID bottom_left_id = 0;

    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar;

        ImGui::Begin("MainDockSpace", nullptr, host_flags);
        ImGui::PopStyleVar(2);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpaceID");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        // One-time DockBuilder layout
        static bool s_dock_init = false;
        if (!s_dock_init)
        {
            s_dock_init = true;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            ImGuiID left_top_id = ImGui::DockBuilderSplitNode(left_id, ImGuiDir_Up, 0.5f, nullptr, &left_bottom_id);
            ImGuiID bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.20f, nullptr, &dock_main_id);
            ImGuiID bottom_right_id = ImGui::DockBuilderSplitNode(bottom_id, ImGuiDir_Right, 0.65f, nullptr, &bottom_left_id);

            // Dock windows by exact names used below
            ImGui::DockBuilderDockWindow("Light Control", left_top_id);
            ImGui::DockBuilderDockWindow("Renderer Settings", left_bottom_id);
            ImGui::DockBuilderDockWindow("Post Processing", left_bottom_id);
            ImGui::DockBuilderDockWindow("Scene", dock_main_id);
            ImGui::DockBuilderDockWindow("Reload Shaders", bottom_left_id);
            ImGui::DockBuilderDockWindow("Reload Assets", bottom_left_id);
            ImGui::DockBuilderDockWindow("Console", bottom_right_id);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End(); // End MainDockSpace
    }

    // --------------------- Light Control --------------------------
    ImGui::Begin("Light Control");

    ImGui::Text("Point Lights");
    for (int i = 0; i < scene.GetPointLights().size(); ++i)
    {
        scene.GetPointLights()[i].pointOnImGuiRender(i);
    }

    ImGui::Text("Dir Lights");
    scene.GetDirLight().dirOnImGuiRender();

    ImGui::End();

    // --------------------- Post Processing --------------------------
    ImGui::Begin("Post Processing");
    ImGui::Checkbox("usePost", &usePostProcess);

    if (usePostProcess)
    {
        ImGui::Checkbox("useHdr", &useHdr);
        if (useHdr)
            ImGui::SliderFloat("Exposure", &exposure, 0.01f, 10.0f);

        if (drawLights)
        {
            ImGui::Checkbox("useBloom", &useBloom);
            if (useBloom)
                ImGui::SliderFloat("samplerDistance", &samplerDistance, 0.01f, 10.0f);
        }

        ImGui::Combo("Effect Mode", &effectMode, "normal\0inversion\0grayscale\0sharpen\0blur\0\0");
        if (effectMode == 3 || effectMode == 4)
        {
            ImGui::SliderFloat("Offset", &offset, 100.0f, 1000.0f);
        }
        ImGui::SliderFloat("Scan Pos", &scanPos, 0.0f, static_cast<float>(framebufferWidth));
    }

    ImGui::End();

    // --------------------- Renderer Settings --------------------------
    ImGui::Begin("Renderer Settings");

    ImGui::SliderFloat("aoBias", &aoBias, -1.0f, 1.0f);
    ImGui::SliderFloat("roughnessBias", &roughnessBias, -1.0f, 1.0f);
    ImGui::SliderFloat("metallicBias", &metallicBias, -1.0f, 1.0f);
    input.onImGuiRender();

    ImGui::Checkbox("useNormal", &hasNormal);
    ImGui::SameLine();
    ImGui::Checkbox("useShadow", &useShadows);
    ImGui::SameLine();
    ImGui::Checkbox("useDeferred", &useDeferred);

    ImGui::Checkbox("drawLights", &drawLights);
    ImGui::SameLine();
    ImGui::Checkbox("drawPlane", &drawPlane);
    ImGui::SameLine();
    ImGui::Checkbox("drawDebug", &drawDebug);

    if (!scene.GetObjects().empty())
        scene.GetObjects()[0].transform.onImGuiRender();

    ImGui::End();

    // ------------------- Reload Shaders -------------------------
    ImGui::Begin("Reload Shaders");
    static std::string lastReloadMsg = "Idle";

    if (ImGui::Button("Reload Shaders")) {
        auto results = ResourceManager::GetInstance().ReloadAllShaders();
        std::string msg;

        auto applyDefaults = [&](const std::string& name) {
            if (name == "model" && modelShader) {
                modelShader->use();
                modelShader->setUniform("depthMap", 0);
                for (int i = 0; i < 4; ++i)
                {
                    modelShader->setUniform("shadowMap[" + std::to_string(i) + "]", 1 + i);
                }
                modelShader->setUniform("diffuse", 5);
                modelShader->setUniform("specular", 6);
                modelShader->setUniform("normal", 7);
                modelShader->setUniform("height", 8);
                modelShader->setUniform("arm", 9);
                modelShader->setUniform("irradianceMap", 11);
                modelShader->setUniform("prefilterMap", 12);
                modelShader->setUniform("brdfLUT", 13);
                // point light constants previously set in init
                for (int i = 0; i < static_cast<int>(scene.GetPointLights().size()); ++i) {
                    std::string base = "pointLight[" + std::to_string(i) + "]";
                    modelShader->setUniform(base + ".constant", 1.0f);
                    modelShader->setUniform(base + ".linear", 0.09f);
                    modelShader->setUniform(base + ".quadratic", 0.032f);
                }
            }
            else if (name == "scene framebuffer" && sceneFramebufferShader) {
                sceneFramebufferShader->use();
                sceneFramebufferShader->setUniform("screenTexture", 0);
                sceneFramebufferShader->setUniform("blur", 1);
                sceneFramebufferShader->setUniform("useGamma", useGamma);
                sceneFramebufferShader->setUniform("useHdr", useHdr);
                sceneFramebufferShader->setUniform("useBloom", useBloom);
                sceneFramebufferShader->setUniform("exposure", exposure);
            }
            else if (name == "bloomBlur" && bloomBlurShader) {
                bloomBlurShader->use();
                bloomBlurShader->setUniform("image", 0);
            }
            else if (name == "background" && backgroundShader) {
                backgroundShader->use();
                backgroundShader->setUniform("environmentMap", 0);
            }
            else if (name == "lightPass" && lightPassShader) {
                lightPassShader->use();
                lightPassShader->setUniform("gPosition", 0);
                lightPassShader->setUniform("gNormal", 1);
                lightPassShader->setUniform("gAlbedo", 2);
                lightPassShader->setUniform("gARM", 3);
                lightPassShader->setUniform("gGeoNormal", 4);
                lightPassShader->setUniform("gDepth", 5);
                lightPassShader->setUniform("depthMap", 6);
                for (int i = 0; i < 4; ++i)
                {
                    lightPassShader->setUniform("shadowMap[" + std::to_string(i) + "]", 7 + i);
                }
                lightPassShader->setUniform("irradianceMap", 11);
                lightPassShader->setUniform("prefilterMap", 12);
                lightPassShader->setUniform("brdfLUT", 13);
                for (int i = 0; i < static_cast<int>(scene.GetPointLights().size()); ++i) {
                    std::string base = "pointLight[" + std::to_string(i) + "]";
                    lightPassShader->setUniform(base + ".constant", 1.0f);
                    lightPassShader->setUniform(base + ".linear", 0.09f);
                    lightPassShader->setUniform(base + ".quadratic", 0.032f);
                }
            }
            else if (name == "irradiance" && irradianceShader) {
                irradianceShader->use();
                irradianceShader->setUniform("environmentMap", 0);
            }
            else if (name == "prefilter" && prefilterShader) {
                prefilterShader->use();
                prefilterShader->setUniform("environmentMap", 0);
            }
        };

        for (const auto& r : results) {
            msg += r.first + (r.second ? ": reloaded\n" : ": no change or failed\n");
            if (r.second) applyDefaults(r.first);
        }
        lastReloadMsg = msg;
    }

    ImGui::Separator();
    ImGui::TextWrapped("%s", lastReloadMsg.c_str());
    ImGui::End();

    // ------------------- Hot Reload Assets ----------------------
    ImGui::Begin("Reload Assets");
    static char modelPathBuf[1024] = "../assets/models/blue_metal_plate_4k.gltf/blue_metal_plate_4k.gltf";
    static char hdrPathBuf[1024] = "../assets/hdr/newman_cafeteria_4k.hdr";
    static std::string hotReloadMsg = "Idle";

    ImGui::InputText("Model Path", modelPathBuf, sizeof(modelPathBuf));
    //ImGui::SameLine();
    if (ImGui::Button("Browse Model...")) {
        // Prepare a FileDialogConfig and set the path
        IGFD::FileDialogConfig cfg;
        cfg.path = "../assets"; // starting directory (can be changed)

        // Open file dialog
        ImGuiFileDialog::Instance()->OpenDialog("ChooseModelDlg", "Choose Model", ".gltf,.glb", cfg);
    }

    ImGui::SetNextWindowDockID(left_bottom_id, ImGuiCond_FirstUseEver);
    //ImGui::SetNextWindowSize(ImVec2(600, 600), ImGuiCond_FirstUseEver);

    // Display model file dialog and auto-load on selection
    if (ImGuiFileDialog::Instance()->Display("ChooseModelDlg")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string chosen = ImGuiFileDialog::Instance()->GetFilePathName();
            SafeCopyPath(modelPathBuf, sizeof(modelPathBuf), chosen);
            modelPathBuf[sizeof(modelPathBuf)-1] = '\0';

            // Load/replace model immediately (replace first object)
            auto& res = ResourceManager::GetInstance();
            auto mdl = res.LoadModel(chosen);
            if (mdl) {
                if (!scene.GetObjects().empty())
                    scene.GetObjects()[0].model = mdl;
                else
                    scene.AddObject(mdl);
                hotReloadMsg = "Model loaded: " + chosen;
            } else {
                hotReloadMsg = "Failed to load model: " + chosen;
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reload Model (same path)")) {
        std::string path(modelPathBuf);
        bool ok = ResourceManager::GetInstance().ReloadModel(path);
        hotReloadMsg = ok ? ("Reloaded model: " + path) : ("Reload model failed: " + path);
    }

    ImGui::Separator();

    ImGui::InputText("HDR Path", hdrPathBuf, sizeof(hdrPathBuf));
    //ImGui::SameLine();
    if (ImGui::Button("Browse HDR...")) {
        IGFD::FileDialogConfig cfg2;
        cfg2.path = "../assets";
        ImGuiFileDialog::Instance()->OpenDialog("ChooseHdrDlg", "Choose HDR", ".hdr", cfg2);
    }

    ImGui::SetNextWindowDockID(left_bottom_id, ImGuiCond_FirstUseEver);

    if (ImGuiFileDialog::Instance()->Display("ChooseHdrDlg")) {
        if (ImGuiFileDialog::Instance()->IsOk()) {
            std::string chosen = ImGuiFileDialog::Instance()->GetFilePathName();
            SafeCopyPath(hdrPathBuf, sizeof(hdrPathBuf), chosen);
            hdrPathBuf[sizeof(hdrPathBuf)-1] = '\0';

            auto& res = ResourceManager::GetInstance();
            auto tex = res.LoadTexture(chosen, HDR);
            if (tex) {
                if (!scene.GetEnvironment().asset) {
                    auto asset = std::make_shared<EnvironmentAsset>();
                    scene.SetEnvironment(asset);
                }
                scene.GetEnvironment().asset->hdrTexture = tex->getID();
                scene.GetEnvironment().maps.isGenerated = false;
                hotReloadMsg = "HDR loaded: " + chosen;
            } else {
                hotReloadMsg = "Failed to load HDR: " + chosen;
            }
        }
        ImGuiFileDialog::Instance()->Close();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reload HDR (same path)")) {
        std::string path(hdrPathBuf);
        auto tex = ResourceManager::GetInstance().ReloadTexture(path, HDR);
        if (tex) {
            if (!scene.GetEnvironment().asset) {
                auto asset = std::make_shared<EnvironmentAsset>();
                scene.SetEnvironment(asset);
            }
            scene.GetEnvironment().asset->hdrTexture = tex->getID();
            scene.GetEnvironment().maps.isGenerated = false;
            hotReloadMsg = "Reloaded HDR: " + path;
        } else {
            hotReloadMsg = "Reload HDR failed: " + path;
        }
    }

    ImGui::NewLine();
    ImGui::TextWrapped("%s", hotReloadMsg.c_str());
    ImGui::End();

    // ------------ Console ----------------
    ImGui::Begin("Console");

    if (ImGui::Button("Clear"))
    {
        buffer.str("");
        buffer.clear();
    }

    std::string output = buffer.str();
    ImGui::TextUnformatted(output.c_str());

    ImGui::End();

    // ------------ Scene ----------------
    ImGui::Begin("Scene");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    bool sceneHovered = ImGui::IsWindowHovered();
    input.setSceneHovered(sceneHovered);
    ImVec2 size = ImGui::GetContentRegionAvail();

    unsigned int newWidth = (unsigned int)size.x;
    unsigned int newHeight = (unsigned int)size.y;

    if (newWidth > 0 && newHeight > 0 &&
        (newWidth != framebufferWidth || newHeight != framebufferHeight)
        && !ImGui::IsMouseDown(0))
    {
        resizeFrameBuffer(newWidth, newHeight);
    }

    if (finalTexture != 0)
    {
        ImVec2 imagePos = ImGui::GetCursorScreenPos();

        ImGui::Image(
            (ImTextureID)(intptr_t)finalTexture,
            size,
            ImVec2(0, 1),   // flip vertically
            ImVec2(1, 0)
        ); 
        if (drawDebug && useDeferred)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const char* labels[] = {
                "Normal",
                "Roughness",
                "Metallic",
                "Depth"
            };

            float debugH = size.y / 4.0f;
            float padding = 6.0f;

            for (int i = 0; i < 4; i++)
            {
                float x = imagePos.x + 10.0f;
                float y = imagePos.y + i * debugH + 10.0f;

                // background (clearer/readable)
                ImVec2 bgMin(x - padding, y - padding);
                ImVec2 bgMax(x + 75.0f, y + 18.0f);

                drawList->AddRectFilled(
                    bgMin,
                    bgMax,
                    IM_COL32(0, 0, 0, 150)
                );

                // text
                drawList->AddText(
                    ImVec2(x, y),
                    IM_COL32(255, 255, 0, 255),
                    labels[i]
                );
            }
        }
    }
    else
    {
        ImGui::Text("No Render Output");
    }

    ImGui::End();
}

