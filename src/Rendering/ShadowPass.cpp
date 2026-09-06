#include "Rendering/RenderPasses.h"
#include "Rendering/DrawHelpers.h"
#include "Mesh.h"
#include <string>

namespace Rendering
{
ShadowPass::ShadowPass(ResourceManager &resources)
    : dirShadowShader(RequireShader(resources, "dir shadow")),
      pointShadowShader(RequireShader(resources, "point shadow"))
{
}

void ShadowPass::execute(const RenderScene &scene, const RenderPassContext &context, ShadowMapView shadows,
                         const Mesh &plane)
{
    const auto &settings = context.settings;
    const auto &frame = context.frame;
    glm::mat4 model(1.0f);

    const FrameBuffer &parallelShadowFrameBuffer = shadows.directional;
    glEnable(GL_DEPTH_TEST);

    glViewport(0, 0, parallelShadowFrameBuffer.getWidth(), parallelShadowFrameBuffer.getHeight());

    const bool directionalLightEnabled = frame.directionalLightEnabled && scene.directionalLight.enabled;
    const bool pointLightEnabled = frame.pointLightsEnabled;
    const bool directionalShadowEnabled = settings.shadows && directionalLightEnabled;
    const bool pointShadowEnabled = settings.shadows && pointLightEnabled;
    const std::size_t pointLightCount = PointLightCount(scene);

    if (directionalShadowEnabled)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, parallelShadowFrameBuffer.getFBO());
        glClear(GL_DEPTH_BUFFER_BIT);

        dirShadowShader->use();

        dirShadowShader->setUniform("lightSpaceMatrix", context.lightSpaceMatrix);
        dirShadowShader->setUniform("model", model);
        glDisable(GL_CULL_FACE);
        plane.draw();
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        for (const auto &obj : scene.objects)
        {
            if (!obj.model)
                continue;
            renderModel(obj.transform, *obj.model, *dirShadowShader);
        }
        glCullFace(GL_BACK);
    }

    if (pointShadowEnabled)
    {
        pointShadowShader->use();

        for (std::size_t i = 0; i < pointLightCount; ++i)
        {
            if (!scene.pointLights[i].enabled)
                continue;

            const FrameBuffer &fbo = *shadows.points[i];
            glViewport(0, 0, fbo.getWidth(), fbo.getHeight());

            glBindFramebuffer(GL_FRAMEBUFFER, fbo.getFBO());
            glClear(GL_DEPTH_BUFFER_BIT);

            for (GLuint j = 0; j < 6; ++j)
            {
                pointShadowShader->setUniform("shadowMatrices[" + std::to_string(j) + "]",
                                              scene.pointLights[i].shadowMatrices[j]);
            }

            pointShadowShader->setUniform("far_plane", scene.pointLights[i].farPlane);
            pointShadowShader->setUniform("lightPos", scene.pointLights[i].position);

            pointShadowShader->setUniform("model", model);
            glDisable(GL_CULL_FACE);
            plane.draw();
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            for (const auto &obj : scene.objects)
            {
                if (!obj.model)
                    continue;
                renderModel(obj.transform, *obj.model, *pointShadowShader);
            }
            glCullFace(GL_BACK);
        }
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, context.extent.width, context.extent.height);
}
} // namespace Rendering
