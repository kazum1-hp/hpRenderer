#include "Rendering/DrawHelpers.h"
#include "ResourceManager.h"
#include "Model.h"
#include "RenderLimits.h"
#include <algorithm>
#include <stdexcept>

namespace Rendering
{
std::size_t PointLightCount(const RenderScene &scene)
{
    return std::min(scene.pointLights.size(), RenderLimits::MaxPointLights);
}

std::shared_ptr<Shader> RequireShader(ResourceManager &resources, const char *name)
{
    auto shader = resources.GetShader(name);
    GLint linked = GL_FALSE;
    if (shader && shader->ID)
        glGetProgramiv(shader->ID, GL_LINK_STATUS, &linked);
    if (!linked)
        throw std::runtime_error(std::string("Render pass requires linked shader: ") + name);
    return shader;
}

void drawMesh(const Mesh &mesh, Shader &shader, bool useNormalMap, bool useHeightMap, bool useARMMap)
{
    for (GLuint slot = 5; slot <= 9; ++slot)
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    bool hasNormalTexture = false;
    bool hasHeightTexture = false;
    bool hasARMTexture = false;

    for (const auto &tex : mesh.getTexture())
    {
        GLuint slot = 0;
        std::string uniformName;

        switch (tex->getType())
        {
        case TextureType::Diffuse:
            uniformName = "diffuse";
            slot = 5;
            break;
        case TextureType::Specular:
            uniformName = "specular";
            slot = 6;
            break;
        case TextureType::Normal:
            uniformName = "normal";
            slot = 7;
            hasNormalTexture = true;
            break;
        case TextureType::Height:
            uniformName = "height";
            slot = 8;
            hasHeightTexture = true;
            break;
        case TextureType::ARM:
            uniformName = "arm";
            slot = 9;
            hasARMTexture = true;
            break;
        default:
            continue;
        }

        tex->bind(slot);
        shader.setUniform(uniformName, static_cast<int>(slot));
    }

    shader.setUniform("hasNormalMap", useNormalMap && hasNormalTexture);
    shader.setUniform("hasHeightMap", useHeightMap && hasHeightTexture);
    shader.setUniform("hasARMMap", useARMMap && hasARMTexture);
}
void drawModel(const Model &model, Shader &shader, bool useNormalMap, bool useHeightMap, bool useARMMap)
{
    for (const auto &mesh : model.meshes)
    {
        drawMesh(*mesh, shader, useNormalMap, useHeightMap, useARMMap);
        if (model.instancingEnabled)
            mesh->drawInstanced(static_cast<int>(model.instanceCount));
        else
            mesh->draw();
    }
}
void renderModel(const glm::mat4 &transform, const Model &model, Shader &shader)
{
    const glm::mat4 &modelMatrix = transform;

    shader.setUniform("model", modelMatrix);

    model.draw();
}
} // namespace Rendering
