#include "Rendering/RenderPasses.h"
#include "Rendering/DrawHelpers.h"
#include "Mesh.h"

namespace Rendering
{
GBufferPass::GBufferPass(ResourceManager &resources) : gBufferShader(RequireShader(resources, "gBuffer"))
{
}

void GBufferPass::execute(const RenderScene &scene, const RenderPassContext &context, const FrameBuffer &output,
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
        drawModel(*obj.model, *gBufferShader, obj.material.useNormalMap, false, true);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
} // namespace Rendering
