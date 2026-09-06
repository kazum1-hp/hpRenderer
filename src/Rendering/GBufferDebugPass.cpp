#include "Rendering/RenderPasses.h"
#include "Rendering/DrawHelpers.h"
#include "Mesh.h"

namespace Rendering
{
GBufferDebugPass::GBufferDebugPass(ResourceManager &resources)
    : gbufferDebugShader(RequireShader(resources, "gbuffer debug"))
{
}

void GBufferDebugPass::execute(const RenderPassContext &context, const FrameBuffer &gbuffer, const FrameBuffer &output,
                               const Mesh &screenQuad)
{
    glBindFramebuffer(GL_FRAMEBUFFER, output.getFBO());
    glViewport(0, 0, context.extent.width, context.extent.height);
    glEnable(GL_DEPTH_TEST);

    const auto &camera = context.camera;
    const auto &settings = context.settings;
    const GLuint gNormal = gbuffer.getColor(1);
    const GLuint gARM = gbuffer.getColor(3);
    const GLuint gDepth = gbuffer.getDepth2D();
    glDisable(GL_DEPTH_TEST);

    if (settings.drawGBufferDebug)
    {
        int debugH = context.extent.height / 4;
        int debugW = debugH * static_cast<int>((static_cast<float>(context.extent.width) / context.extent.height));

        gbufferDebugShader->use();
        glBindVertexArray(screenQuad.getVAO());

        auto drawDebug = [&](GLuint tex, int index) {
            glViewport(0, // x
                       static_cast<int>(context.extent.height) -
                           (index + 1) * debugH, // OpenGL origin is at the lower left.
                       debugW, debugH);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            gbufferDebugShader->setUniform("debugTex", 0);
            gbufferDebugShader->setUniform("u_DebugMode", index);
            gbufferDebugShader->setUniform("fov", camera.fovDegrees);
            gbufferDebugShader->setUniform("nearPlane", camera.nearPlane);
            gbufferDebugShader->setUniform("farPlane", camera.farPlane);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        };

        drawDebug(gNormal, 0);
        drawDebug(gARM, 1);
        drawDebug(gARM, 2);
        drawDebug(gDepth, 3);
    }

    glViewport(0, 0, context.extent.width, context.extent.height);
    glEnable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
} // namespace Rendering
