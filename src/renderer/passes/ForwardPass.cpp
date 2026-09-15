#include "hpr/renderer/passes/RenderPasses.h"
#include "hpr/renderer/passes/DrawHelpers.h"
#include "hpr/renderer/opengl/Mesh.h"
#include "hpr/renderer/opengl/RenderProfiler.h"
#include "hpr/renderer/opengl/GpuModel.h"
#include "hpr/renderer/RenderLimits.h"
#include <string>
#include <algorithm>

namespace Rendering
{
ForwardPass::ForwardPass(AssetManager &resources) : modelShader(RequireShader(resources, ShaderId::Model))
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
    modelShader->setUniform("opacityMap", 6);
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

void ForwardPass::execute(const GpuRenderScene &scene, const RenderPassContext &context, ShadowMapView shadows,
                          const FrameBuffer &output, const Mesh &plane, ForwardPhase phase)
{
    const bool transparentOnly = phase == ForwardPhase::Transparent;
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
    if (!transparentOnly) glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, context.extent.width, context.extent.height);
    // object
    modelShader->use();

    modelShader->setUniform("time", frame.timeSeconds);
    const auto& ambient = scene.ambientLighting;
    modelShader->setUniform("useIBL", ambient.mode == AmbientLightingMode::IBL && context.environment.irradianceMap != 0);
    modelShader->setUniform("useHemisphere", ambient.mode == AmbientLightingMode::Hemisphere);
    modelShader->setUniform("hemisphereIntensity", ambient.hemisphereIntensity);
    modelShader->setUniform("iblIntensity", ambient.iblIntensity);
    modelShader->setUniform("ambientSkyColor", ambient.skyColor);
    modelShader->setUniform("ambientGroundColor", ambient.groundColor);

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
    modelShader->setUniform("directionalShadowInvDepthRange", 1.0f / context.directionalShadowDepthRange);
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

    const auto &env = context.environment;
    glActiveTexture(GL_TEXTURE11);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.irradianceMap);
    glActiveTexture(GL_TEXTURE12);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.prefilterMap);
    glActiveTexture(GL_TEXTURE13);
    glBindTexture(GL_TEXTURE_2D, env.brdfLUT);

    if (!transparentOnly && settings.groundPlane.visible)
    {
        ScopedRenderedObject renderedObject(&plane);
        modelShader->setUniform("aoBias", 0.0f);
        modelShader->setUniform("roughnessBias", 0.0f);
        modelShader->setUniform("metallicBias", 0.0f);
        modelShader->setUniform("model", model);
        modelShader->setUniform("height_scale", settings.groundPlane.heightScale);
        drawMesh(plane, *modelShader, settings.groundPlane.useNormalMap, settings.groundPlane.useHeightMap, false);
        glDisable(GL_CULL_FACE);
        plane.draw();
        glEnable(GL_CULL_FACE);
    }

    struct TransparentDraw { const GpuRenderItem* object; const ModelDraw* draw; float depth; };
    std::vector<TransparentDraw> transparent;
    for (const auto &obj : scene.objects)
    {
        ScopedRenderedObject renderedObject(&obj);
        if (!obj.model)
            continue;
        if (transparentOnly)
        {
            for (const auto& draw : obj.model->data().draws)
            {
                const auto& data = obj.model->data();
                const auto& mesh = data.meshes.at(draw.meshIndex);
                if (obj.model->material(mesh.materialIndex).alphaMode != AlphaMode::Blend) continue;
                const auto world = obj.transform * data.nodes.at(draw.nodeIndex).worldTransform;
                const float depth = (camera.view * world * glm::vec4(mesh.center, 1.0f)).z;
                transparent.push_back({&obj, &draw, depth});
            }
            continue;
        }
        modelShader->setUniform("aoBias", obj.material.aoBias);
        modelShader->setUniform("roughnessBias", obj.material.roughnessBias);
        modelShader->setUniform("metallicBias", obj.material.metallicBias);
        modelShader->setUniform("height_scale", 0.0f);
        modelShader->setUniform("model", obj.transform);
        drawModel(*obj.model, obj.transform, *modelShader, obj.material.useNormalMap);
    }

    if (transparentOnly && !transparent.empty())
    {
        std::stable_sort(transparent.begin(), transparent.end(),
                        [](const auto& a, const auto& b) { return a.depth < b.depth; });
        const GLboolean blend = glIsEnabled(GL_BLEND);
        GLboolean depthMask;
        GLint srcRGB, dstRGB, srcAlpha, dstAlpha, equationRGB, equationAlpha;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);
        glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB); glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &srcAlpha); glGetIntegerv(GL_BLEND_DST_ALPHA, &dstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &equationRGB); glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &equationAlpha);
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        for (const auto& entry : transparent)
        {
            ScopedRenderedObject renderedObject(entry.object);
            const auto& obj = *entry.object;
            modelShader->setUniform("aoBias", obj.material.aoBias);
            modelShader->setUniform("roughnessBias", obj.material.roughnessBias);
            modelShader->setUniform("metallicBias", obj.material.metallicBias);
            modelShader->setUniform("height_scale", 0.0f);
            drawModelPart(*obj.model, *entry.draw, obj.transform, *modelShader, obj.material.useNormalMap);
        }
        glDepthMask(depthMask);
        glBlendEquationSeparate(equationRGB, equationAlpha);
        glBlendFuncSeparate(srcRGB, dstRGB, srcAlpha, dstAlpha);
        if (!blend) glDisable(GL_BLEND);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
} // namespace Rendering
