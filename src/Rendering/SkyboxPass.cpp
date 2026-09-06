#include "Rendering/RenderPasses.h"
#include "Rendering/DrawHelpers.h"
#include "Mesh.h"

namespace Rendering
{
SkyboxPass::SkyboxPass(ResourceManager &resources) : backgroundShader(RequireShader(resources, "background"))
{
    restoreShaderBindings();
}

void SkyboxPass::restoreShaderBindings()
{
    backgroundShader->use();
    backgroundShader->setUniform("environmentMap", 0);
}

void SkyboxPass::execute(const RenderPassContext &context, const FrameBuffer &output, const Mesh &cube, bool usePost)
{
    glBindFramebuffer(GL_FRAMEBUFFER, output.getFBO());
    glViewport(0, 0, context.extent.width, context.extent.height);
    glEnable(GL_DEPTH_TEST);

    const auto &camera = context.camera;
    const auto &env = context.environment;
    glDisable(GL_CULL_FACE); // or glCullFace(GL_FRONT)

    backgroundShader->use();
    backgroundShader->setUniform("view", camera.view);
    backgroundShader->setUniform("projection", camera.projection);
    backgroundShader->setUniform("usePost", usePost);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, env.envCubemap);
    cube.draw();

    glEnable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
} // namespace Rendering
