#include "hpr/renderer/passes/RenderPasses.h"
#include "hpr/renderer/passes/DrawHelpers.h"
#include "hpr/renderer/opengl/Mesh.h"

namespace Rendering
{
GBufferPass::GBufferPass(AssetManager &resources) : gBufferShader(RequireShader(resources, ShaderId::GBuffer))
{
}

void GBufferPass::execute(const GpuRenderScene &scene, const RenderPassContext &context, const FrameBuffer &output,
                          const Mesh &plane)
{
    const auto &settings = context.settings;
    const auto &camera = context.camera;
    glm::mat4 model(1.0f);

    const FrameBuffer &gFrameBuffer = output;
    glEnable(GL_DEPTH_TEST);
    glViewport(0, 0, context.extent.width, context.extent.height);
    GLuint gBuffer = gFrameBuffer.getFBO();

    glBindFramebuffer(GL_FRAMEBUFFER, gBuffer);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    gBufferShader->use();
    // uniform setting
    gBufferShader->setUniform("view", camera.view);
    gBufferShader->setUniform("projection", camera.projection);
    gBufferShader->setUniform("viewPos", camera.position);

    // PBR / ORM settings

    if (settings.groundPlane.visible)
    {
        gBufferShader->setUniform("aoBias", 0.0f);
        gBufferShader->setUniform("roughnessBias", 0.0f);
        gBufferShader->setUniform("metallicBias", 0.0f);
        gBufferShader->setUniform("model", model);
        drawMesh(plane, *gBufferShader, settings.groundPlane.useNormalMap, false, false);
        glDisable(GL_CULL_FACE);
        plane.draw();
        glEnable(GL_CULL_FACE);
    }

    for (const auto &obj : scene.objects)
    {
        if (!obj.model)
            continue;
        gBufferShader->setUniform("aoBias", obj.material.aoBias);
        gBufferShader->setUniform("roughnessBias", obj.material.roughnessBias);
        gBufferShader->setUniform("metallicBias", obj.material.metallicBias);
        gBufferShader->setUniform("model", obj.transform);
        drawModel(*obj.model, obj.transform, *gBufferShader, obj.material.useNormalMap);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
} // namespace Rendering
