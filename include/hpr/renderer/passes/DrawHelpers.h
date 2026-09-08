#pragma once
#include "hpr/renderer/ShaderId.h"
#include "hpr/renderer/RenderScene.h"
#include <memory>

class AssetManager;
class Shader;
class GpuModel;
class Mesh;
struct ModelMaterial;
struct ModelDraw;

namespace Rendering
{
std::size_t PointLightCount(const GpuRenderScene &scene);
std::shared_ptr<Shader> RequireShader(AssetManager &resources, ShaderId id);
void drawMesh(const Mesh &mesh, Shader &shader, bool useNormalMap, bool useHeightMap, bool useARMMap,
              const ModelMaterial* material = nullptr);
void drawModelPart(const GpuModel& model, const ModelDraw& draw, const glm::mat4& transform,
                   Shader& shader, bool useNormalMap);
void drawModel(const GpuModel &model, const glm::mat4& transform, Shader &shader, bool useNormalMap);
void renderModel(const glm::mat4 &transform, const GpuModel &model, Shader &shader);
} // namespace Rendering
