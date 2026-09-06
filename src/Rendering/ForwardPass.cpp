#include "Rendering/RenderPasses.h"
#include "Rendering/DrawHelpers.h"
#include "Mesh.h"
#include "RenderLimits.h"
#include <string>

namespace Rendering
{
ForwardPass::ForwardPass(ResourceManager &resources) : modelShader(RequireShader(resources, "model"))
{
    restoreShaderBindings();
}

void ForwardPass::restoreShaderBindings()
{
    modelShader->use();
    modelShader->setUniform("depthMap", 0);
    for (std::size_t i = 0; i < RenderLimits::MaxPointLights; ++i)
    {
        modelShader->setUniform("shadowMap[" + std::to_string(i) + "]", static_cast<int>(1 + i));
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
    for (std::size_t i = 0; i < RenderLimits::MaxPointLights; ++i)
    {
        std::string base = "pointLight[" + std::to_string(i) + "]";
        modelShader->setUniform(base + ".constant", 1.0f);
        modelShader->setUniform(base + ".linear", 0.09f);
        modelShader->setUniform(base + ".quadratic", 0.032f);
    }
}

void ForwardPass::execute(const RenderScene &scene, const RenderPassContext &context, ShadowMapView shadows,
                          const FrameBuffer &output, const Mesh &plane)
{
    const auto &settings = context.settings;
    const auto &camera = context.camera;
    const auto &frame = context.frame;
    glm::mat4 model(1.0f);

    const FrameBuffer &parallelShadowFrameBuffer = shadows.directional;
    const bool directionalLightEnabled = frame.directionalLightEnabled && scene.directionalLight.enabled;
    const bool pointLightEnabled = frame.pointLightsEnabled;
    const bool directionalShadowEnabled = settings.shadows && directionalLightEnabled;
    const bool pointShadowEnabled = settings.shadows && pointLightEnabled;
    const std::size_t pointLightCount = PointLightCount(scene);

    glBindFramebuffer(GL_FRAMEBUFFER, output.getFBO());
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, context.extent.width, context.extent.height);
    // object
    modelShader->use();

    modelShader->setUniform("time", frame.timeSeconds);
    modelShader->setUniform("usePost", settings.postProcess.enabled);

    // transform matrix
    modelShader->setUniform("view", camera.view);
    modelShader->setUniform("projection", camera.projection);
    modelShader->setUniform("viewPos", camera.position);

    // light
    modelShader->setUniform("useQuadratic", settings.quadraticAttenuation);

    // paralleLight
    modelShader->setUniform("parallelLight.color", scene.directionalLight.color);
    modelShader->setUniform("parallelLight.direction", scene.directionalLight.direction);
    modelShader->setUniform("parallelLight.intensity", scene.directionalLight.intensity);
    modelShader->setUniform("parallelLight.enabled", directionalLightEnabled);
    modelShader->setUniform("lightSpaceMatrix", context.lightSpaceMatrix);
    modelShader->setUniform("parallelShadows", directionalShadowEnabled);
    modelShader->setUniform("pointShadows", pointShadowEnabled);
    modelShader->setUniform("pointLightCount", static_cast<int>(pointLightCount));

    if (directionalShadowEnabled)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, parallelShadowFrameBuffer.getDepth2D());

        modelShader->setUniform("depthMap", 0);
    }

    // point light

    for (std::size_t i = 0; i < pointLightCount; ++i)
    {
        std::string base = "pointLight[" + std::to_string(i) + "]";

        modelShader->setUniform(base + ".color", scene.pointLights[i].color);
        modelShader->setUniform(base + ".position", scene.pointLights[i].position);
        modelShader->setUniform(base + ".intensity", scene.pointLights[i].intensity);
        modelShader->setUniform(base + ".enabled", scene.pointLights[i].enabled && pointLightEnabled);
        modelShader->setUniform(base + ".farPlane", scene.pointLights[i].farPlane);

        if (pointShadowEnabled)
        {
            glActiveTexture(GL_TEXTURE1 + i);
            glBindTexture(GL_TEXTURE_CUBE_MAP, shadows.points[i]->getDepthCube());

            modelShader->setUniform("shadowMap[" + std::to_string(i) + "]", static_cast<int>(1 + i));
        }
    }

    if (settings.groundPlane.visible)
    {
        modelShader->setUniform("model", model);
        modelShader->setUniform("height_scale", settings.groundPlane.heightScale);
        drawMesh(plane, *modelShader, settings.groundPlane.useNormalMap, settings.groundPlane.useHeightMap, false);
        glDisable(GL_CULL_FACE);
        plane.draw();
        glEnable(GL_CULL_FACE);
    }

    const auto &env = context.environment;
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.irradianceMap);
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.prefilterMap);
    glActiveTexture(GL_TEXTURE13);
    glBindTexture(GL_TEXTURE_2D, env.brdfLUT);

    for (const auto &obj : scene.objects)
    {
        if (!obj.model)
            continue;
        modelShader->setUniform("aoBias", obj.material.aoBias);
        modelShader->setUniform("roughnessBias", obj.material.roughnessBias);
        modelShader->setUniform("metallicBias", obj.material.metallicBias);
        modelShader->setUniform("height_scale", 0.0f);
        modelShader->setUniform("model", obj.transform);
        drawModel(*obj.model, *modelShader, obj.material.useNormalMap, false, true);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
} // namespace Rendering
