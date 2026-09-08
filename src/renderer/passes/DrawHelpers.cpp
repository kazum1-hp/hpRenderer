#include "hpr/renderer/passes/DrawHelpers.h"
#include "hpr/assets/AssetManager.h"
#include "hpr/renderer/opengl/GpuModel.h"
#include "hpr/renderer/RenderLimits.h"
#include <algorithm>
#include <stdexcept>
#include <cmath>

namespace Rendering
{
std::size_t PointLightCount(const GpuRenderScene &scene)
{
    return std::min(scene.pointLights.size(), RenderLimits::MaxPointLights);
}

std::shared_ptr<Shader> RequireShader(AssetManager &resources, ShaderId id)
{
    auto shader = resources.GetShader(id);
    GLint linked = GL_FALSE;
    if (shader && shader->ID)
        glGetProgramiv(shader->ID, GL_LINK_STATUS, &linked);
    if (!linked)
        throw std::runtime_error(std::string("Render pass requires linked shader: ") + ShaderName(id));
    return shader;
}

void drawMesh(const Mesh &mesh, Shader &shader, bool useNormalMap, bool useHeightMap, bool useARMMap,
              const ModelMaterial* material)
{
    for (GLuint slot : {5u, 6u, 7u, 8u, 9u, 10u, 14u, 15u})
    {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    bool hasNormalTexture = false;
    bool hasHeightTexture = false;
    bool hasARMTexture = false;
    bool hasDiffuse = false, hasAO = false, hasRoughness = false, hasMetallic = false, hasOpacity = false;
    const ModelMaterial fallback;
    const auto& values = material ? *material : fallback;
    shader.setUniform("baseColorFactor", values.baseColor);
    shader.setUniform("roughnessFactor", values.roughness);
    shader.setUniform("metallicFactor", values.metallic);
    shader.setUniform("alphaMode", static_cast<int>(values.alphaMode));
    shader.setUniform("alphaCutoff", values.alphaCutoff);
    shader.setUniform("doubleSided", values.doubleSided);
    shader.setUniform("roughnessChannel", 0);
    shader.setUniform("metallicChannel", 0);
    for (const auto& ref : values.textures)
    {
        if (ref.semantic == Roughness) shader.setUniform("roughnessChannel", ref.channel);
        if (ref.semantic == Metallic) shader.setUniform("metallicChannel", ref.channel);
    }
    // Always initialize sampler units, even when an optional map is absent.
    shader.setUniform("diffuse", 5);
    shader.setUniform("opacityMap", 6);
    shader.setUniform("normal", 7);
    shader.setUniform("height", 8);
    shader.setUniform("arm", 9);
    shader.setUniform("aoMap", 10);
    shader.setUniform("roughnessMap", 14);
    shader.setUniform("metallicMap", 15);

    for (const auto &tex : mesh.getTexture())
    {
        if (!tex || !tex->isValid()) continue;
        GLuint slot = 0;
        std::string uniformName;

        switch (tex->getType())
        {
        case TextureType::Diffuse:
            hasDiffuse = true;
            uniformName = "diffuse";
            slot = 5;
            break;
        case TextureType::Specular:
            continue; // Retained as an asset; the metallic/roughness shader does not use Phong specular.
        case TextureType::Opacity:
            uniformName = "opacityMap";
            slot = 6;
            hasOpacity = true;
            break;
        case TextureType::AmbientOcclusion:
            uniformName = "aoMap";
            slot = 10;
            hasAO = true;
            break;
        case TextureType::Roughness:
            uniformName = "roughnessMap";
            slot = 14;
            hasRoughness = true;
            break;
        case TextureType::Metallic:
            uniformName = "metallicMap";
            slot = 15;
            hasMetallic = true;
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
    shader.setUniform("hasDiffuseMap", hasDiffuse);
    shader.setUniform("hasAOMap", hasAO);
    shader.setUniform("hasRoughnessMap", hasRoughness);
    shader.setUniform("hasMetallicMap", hasMetallic);
    shader.setUniform("hasOpacityMap", hasOpacity);
}
void drawModelPart(const GpuModel& model, const ModelDraw& draw, const glm::mat4& transform,
                   Shader& shader, bool useNormalMap)
{
    const auto& asset = model.data();
    const auto material = model.material(asset.meshes.at(draw.meshIndex).materialIndex);
    const glm::mat4 world = transform * asset.nodes.at(draw.nodeIndex).worldTransform;
    const float determinant = glm::determinant(glm::mat3(world));
    if (!std::isfinite(determinant) || std::abs(determinant) < 1e-12f) return;
    GLint frontFace;
    glGetIntegerv(GL_FRONT_FACE, &frontFace);
    const GLboolean culling = glIsEnabled(GL_CULL_FACE);
    if (material.doubleSided) glDisable(GL_CULL_FACE);
    if (determinant < 0.0f) glFrontFace(frontFace == GL_CCW ? GL_CW : GL_CCW);
    shader.setUniform("model", world);
    const auto& mesh = model.mesh(draw.meshIndex);
    drawMesh(mesh, shader, useNormalMap && asset.meshes.at(draw.meshIndex).hasTangents, false, true, &material);
    mesh.draw();
    glFrontFace(frontFace);
    if (culling) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
}
void drawModel(const GpuModel &model, const glm::mat4& transform, Shader &shader, bool useNormalMap)
{
    for (const auto& draw : model.data().draws)
    {
        const auto& source = model.data().meshes.at(draw.meshIndex);
        if (model.material(source.materialIndex).alphaMode == AlphaMode::Blend) continue;
        drawModelPart(model, draw, transform, shader, useNormalMap);
    }
}
void renderModel(const glm::mat4 &transform, const GpuModel &model, Shader &shader)
{
    // Blended surfaces do not cast opaque shadow-map silhouettes; MASK materials do.
    drawModel(model, transform, shader, false);
}
} // namespace Rendering
