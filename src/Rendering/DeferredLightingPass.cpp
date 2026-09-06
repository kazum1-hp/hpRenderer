#include "Rendering/RenderPasses.h"
#include "Rendering/DrawHelpers.h"
#include "Mesh.h"
#include "RenderLimits.h"
#include <string>

namespace Rendering
{
DeferredLightingPass::DeferredLightingPass(ResourceManager &resources)
    : lightPassShader(RequireShader(resources, "lightPass"))
{
    restoreShaderBindings();
}

void DeferredLightingPass::restoreShaderBindings()
{
    lightPassShader->use();
    lightPassShader->setUniform("gPosition", 0);
    lightPassShader->setUniform("gNormal", 1);
    lightPassShader->setUniform("gAlbedo", 2);
    lightPassShader->setUniform("gARM", 3);
    lightPassShader->setUniform("gGeoNormal", 4);
    lightPassShader->setUniform("gDepth", 5);
    lightPassShader->setUniform("depthMap", 6);
    for (std::size_t i = 0; i < RenderLimits::MaxPointLights; ++i)
    {
        lightPassShader->setUniform("shadowMap[" + std::to_string(i) + "]", static_cast<int>(7 + i));
    }
    lightPassShader->setUniform("irradianceMap", 11);
    lightPassShader->setUniform("prefilterMap", 12);
    lightPassShader->setUniform("brdfLUT", 13);
    for (std::size_t i = 0; i < RenderLimits::MaxPointLights; ++i)
    {
        std::string base = "pointLight[" + std::to_string(i) + "]";
        lightPassShader->setUniform(base + ".constant", 1.0f);
        lightPassShader->setUniform(base + ".linear", 0.09f);
        lightPassShader->setUniform(base + ".quadratic", 0.032f);
    }
}

void DeferredLightingPass::execute(const RenderScene &scene, const RenderPassContext &context, ShadowMapView shadows,
                                   const FrameBuffer &gbuffer, const FrameBuffer &output, const Mesh &screenQuad)
{
    const auto &settings = context.settings;
    const auto &camera = context.camera;
    const auto &frame = context.frame;
    const FrameBuffer &parallelShadowFrameBuffer = shadows.directional;
    const FrameBuffer &gFrameBuffer = gbuffer;
    const FrameBuffer &lightPassFrameBuffer = output;
    glViewport(0, 0, context.extent.width, context.extent.height);
    glEnable(GL_DEPTH_TEST);
    const bool directionalLightEnabled = frame.directionalLightEnabled && scene.directionalLight.enabled;
    const bool pointLightEnabled = frame.pointLightsEnabled;
    const bool directionalShadowEnabled = settings.shadows && directionalLightEnabled;
    const bool pointShadowEnabled = settings.shadows && pointLightEnabled;
    const std::size_t pointLightCount = PointLightCount(scene);

    GLuint gBuffer = gFrameBuffer.getFBO();
    GLuint gPosition = gFrameBuffer.getColor(0);
    GLuint gNormal = gFrameBuffer.getColor(1);
    GLuint gAlbedo = gFrameBuffer.getColor(2);
    GLuint gARM = gFrameBuffer.getColor(3);
    GLuint gGeoNormal = gFrameBuffer.getColor(4);
    GLuint gDepth = gFrameBuffer.getDepth2D();

    glBindFramebuffer(GL_FRAMEBUFFER, lightPassFrameBuffer.getFBO());
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

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
    lightPassShader->setUniform("viewPos", camera.position);

    // light
    lightPassShader->setUniform("useQuadratic", settings.quadraticAttenuation);

    // paralleLight
    lightPassShader->setUniform("parallelLight.color", scene.directionalLight.color);
    lightPassShader->setUniform("parallelLight.direction", scene.directionalLight.direction);
    lightPassShader->setUniform("parallelLight.intensity", scene.directionalLight.intensity);
    lightPassShader->setUniform("parallelLight.enabled", directionalLightEnabled);
    lightPassShader->setUniform("lightSpaceMatrix", context.lightSpaceMatrix);
    lightPassShader->setUniform("parallelShadows", directionalShadowEnabled);
    lightPassShader->setUniform("pointShadows", pointShadowEnabled);
    lightPassShader->setUniform("pointLightCount", static_cast<int>(pointLightCount));

    if (directionalShadowEnabled)
    {
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, parallelShadowFrameBuffer.getDepth2D());

        lightPassShader->setUniform("depthMap", 6);
    }

    // point light

    for (std::size_t i = 0; i < pointLightCount; ++i)
    {
        std::string base = "pointLight[" + std::to_string(i) + "]";

        lightPassShader->setUniform(base + ".color", scene.pointLights[i].color);
        lightPassShader->setUniform(base + ".position", scene.pointLights[i].position);
        lightPassShader->setUniform(base + ".intensity", scene.pointLights[i].intensity);
        lightPassShader->setUniform(base + ".enabled", scene.pointLights[i].enabled && pointLightEnabled);
        lightPassShader->setUniform(base + ".farPlane", scene.pointLights[i].farPlane);

        if (pointShadowEnabled)
        {
            glActiveTexture(GL_TEXTURE7 + i);
            glBindTexture(GL_TEXTURE_CUBE_MAP, shadows.points[i]->getDepthCube());

            lightPassShader->setUniform("shadowMap[" + std::to_string(i) + "]", static_cast<int>(7 + i));
        }
    }

    // bind IBL environment maps (same units as modelShader)
    const auto &env = context.environment;
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.irradianceMap);
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.prefilterMap);
    glActiveTexture(GL_TEXTURE13);
    glBindTexture(GL_TEXTURE_2D, env.brdfLUT);

    // screen quad draw
    screenQuad.draw();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, gBuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, lightPassFrameBuffer.getFBO());
    glBlitFramebuffer(0, 0, context.extent.width, context.extent.height, 0, 0, context.extent.width,
                      context.extent.height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, lightPassFrameBuffer.getFBO());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
} // namespace Rendering
