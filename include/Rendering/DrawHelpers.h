#pragma once
#include "RenderScene.h"
#include <memory>

class ResourceManager;
class Shader;
class Model;
class Mesh;

namespace Rendering
{
std::size_t PointLightCount(const RenderScene &scene);
std::shared_ptr<Shader> RequireShader(ResourceManager &resources, const char *name);
void drawMesh(const Mesh &mesh, Shader &shader, bool useNormalMap, bool useHeightMap, bool useARMMap);
void drawModel(const Model &model, Shader &shader, bool useNormalMap, bool useHeightMap, bool useARMMap);
void renderModel(const glm::mat4 &transform, const Model &model, Shader &shader);
} // namespace Rendering
