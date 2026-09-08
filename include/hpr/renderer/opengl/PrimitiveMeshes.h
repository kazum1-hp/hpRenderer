#pragma once
#include <memory>
#include <vector>
class Mesh;
class Texture;
namespace Rendering
{
// Factories create caller-owned GPU meshes. No asset cache, paths or global state.
std::shared_ptr<Mesh> CreateScreenQuad();
std::shared_ptr<Mesh> CreatePlane(const std::vector<std::shared_ptr<Texture>>& textures = {});
std::shared_ptr<Mesh> CreateCube();
} // namespace Rendering
