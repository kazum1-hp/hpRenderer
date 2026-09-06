#include "Rendering/RenderPasses.h"
#include "Rendering/DrawHelpers.h"
#include "Mesh.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Rendering
{
LightMarkerPass::LightMarkerPass(ResourceManager &resources) : lightShader(RequireShader(resources, "light"))
{
}

void LightMarkerPass::execute(const RenderScene &scene, const RenderPassContext &context, const FrameBuffer &output,
                              const Mesh &cube)
{
    glBindFramebuffer(GL_FRAMEBUFFER, output.getFBO());
    glViewport(0, 0, context.extent.width, context.extent.height);
    glEnable(GL_DEPTH_TEST);

    const auto &camera = context.camera;
    const auto &frame = context.frame;
    const glm::mat4 model(1.0f);
    const auto pointLightCount = PointLightCount(scene);
    // draw cube lights
    lightShader->use();

    ////// transform matrix
    lightShader->setUniform("view", camera.view);
    lightShader->setUniform("projection", camera.projection);

    for (std::size_t i = 0; i < pointLightCount; ++i)
    {
        glm::mat4 lightModel(1.0f);
        lightModel = glm::translate(model, scene.pointLights[i].position);
        lightModel = glm::scale(lightModel, glm::vec3(0.1f));
        lightShader->setUniform("model", lightModel);
        lightShader->setUniform("lightColor", scene.pointLights[i].color);
        lightShader->setUniform("enabled", scene.pointLights[i].enabled && frame.pointLightsEnabled);
        if (scene.pointLights[i].enabled && frame.pointLightsEnabled)
        {
            cube.draw();
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
} // namespace Rendering
