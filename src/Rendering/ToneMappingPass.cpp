#include "Rendering/RenderPasses.h"
#include "Rendering/DrawHelpers.h"
#include "Mesh.h"

namespace Rendering
{
ToneMappingPass::ToneMappingPass(ResourceManager &resources)
    : sceneFramebufferShader(RequireShader(resources, "scene framebuffer"))
{
}

void ToneMappingPass::execute(const RenderPassContext &context, GLuint sceneTexture, GLuint bloomTexture,
                              const FrameBuffer &output, const Mesh &screenQuad)
{
    const auto &settings = context.settings;
    const auto &frame = context.frame;
    glDisable(GL_DEPTH_TEST);

    glBindFramebuffer(GL_FRAMEBUFFER, output.getFBO());
    glViewport(0, 0, context.extent.width, context.extent.height);

    sceneFramebufferShader->use();

    sceneFramebufferShader->setUniform("effectMode", settings.postProcess.effectMode);
    sceneFramebufferShader->setUniform("toneMappingMode", settings.postProcess.toneMappingMode);
    sceneFramebufferShader->setUniform("offset", settings.postProcess.kernelOffset);
    sceneFramebufferShader->setUniform("screenTexture", 0);
    sceneFramebufferShader->setUniform("blur", 1);
    sceneFramebufferShader->setUniform("scanPos", settings.postProcess.scanPosition);
    sceneFramebufferShader->setUniform("useHdr", settings.postProcess.hdr);
    sceneFramebufferShader->setUniform("useBloom", settings.postProcess.bloom);
    sceneFramebufferShader->setUniform("exposure", settings.postProcess.exposure);
    sceneFramebufferShader->setUniform("time", frame.timeSeconds);
    sceneFramebufferShader->setUniform("viewportWidth", static_cast<float>(context.extent.width));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneTexture);

    if (settings.postProcess.bloom)
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
