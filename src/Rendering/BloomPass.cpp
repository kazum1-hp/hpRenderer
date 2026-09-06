#include "Rendering/RenderPasses.h"
#include "Rendering/DrawHelpers.h"
#include "Mesh.h"

namespace Rendering
{
BloomPass::BloomPass(ResourceManager &resources) : bloomBlurShader(RequireShader(resources, "bloomBlur"))
{
    restoreShaderBindings();
}

void BloomPass::restoreShaderBindings()
{
    bloomBlurShader->use();
    bloomBlurShader->setUniform("image", 0);
}

GLuint BloomPass::execute(const RenderPassContext &context, GLuint brightTexture,
                          const std::array<std::unique_ptr<FrameBuffer>, 2> &pingPong, const Mesh &screenQuad)
{
    const auto &settings = context.settings;
    glDisable(GL_DEPTH_TEST);
    glViewport(0, 0, context.extent.width, context.extent.height);
    bool horizontal = true, first_iteration = true;
    constexpr unsigned int amount = 5;
    bloomBlurShader->use();
    bloomBlurShader->setUniform("samplerDistance", settings.postProcess.bloomSampleDistance);

    for (unsigned int i = 0; i < amount; i++)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, pingPong[horizontal]->getFBO());
        bloomBlurShader->setUniform("horizontal", horizontal);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(
            GL_TEXTURE_2D,
            first_iteration
                ? brightTexture
                : pingPong[!horizontal]->getColor()); // bind texture of other framebuffer (or scene if first iteration)
        screenQuad.draw();
        horizontal = !horizontal;
        if (first_iteration)
            first_iteration = false;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnable(GL_DEPTH_TEST);
    return pingPong[!horizontal]->getColor();
}
} // namespace Rendering
