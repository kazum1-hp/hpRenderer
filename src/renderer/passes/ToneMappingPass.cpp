#include "hpr/renderer/passes/RenderPasses.h"
#include "hpr/renderer/passes/DrawHelpers.h"
#include "hpr/renderer/opengl/Mesh.h"

namespace Rendering
{
ToneMappingPass::ToneMappingPass(AssetManager &resources)
    : sceneFramebufferShader(RequireShader(resources, ShaderId::ToneMapping))
{
}

void ToneMappingPass::execute(const RenderPassContext &context, GLuint sceneTexture, GLuint bloomTexture,
                              const FrameBuffer &output, const Mesh &screenQuad)
{
    const PostProcessSettings defaults;
    const auto &post = context.settings.postProcess.enabled ? context.settings.postProcess : defaults;
    const auto &frame = context.frame;
    glDisable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, output.getFBO());
    glViewport(0, 0, context.extent.width, context.extent.height);

    sceneFramebufferShader->use();

    sceneFramebufferShader->setUniform("effectMode", post.effectMode);
    sceneFramebufferShader->setUniform("toneMappingMode", post.toneMappingMode);
    sceneFramebufferShader->setUniform("offset", post.kernelOffset);
    sceneFramebufferShader->setUniform("screenTexture", 0);
    sceneFramebufferShader->setUniform("blur", 1);
    sceneFramebufferShader->setUniform("scanPos", post.scanPosition);
    sceneFramebufferShader->setUniform("useHdr", post.hdr);
    sceneFramebufferShader->setUniform("useBloom", post.bloom);
    sceneFramebufferShader->setUniform("exposure", post.exposure);
    sceneFramebufferShader->setUniform("time", frame.timeSeconds);
    sceneFramebufferShader->setUniform("viewportWidth", static_cast<float>(context.extent.width));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);

    if (post.bloom)
    {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomTexture);
    }

    screenQuad.draw();

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_DEPTH_BUFFER_BIT);
}
} // namespace Rendering
