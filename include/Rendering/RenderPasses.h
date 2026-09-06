#pragma once
#include "Rendering/RenderPassContext.h"
#include "Shader.h"
#include <array>

class ResourceManager;
class Mesh;

namespace Rendering
{
// Concrete passes: each owns its shader references and receives explicit targets.
// execute() establishes its framebuffer/viewport/depth state; inputs are never retained.
class ShadowPass
{
  public:
    explicit ShadowPass(ResourceManager &resources);
    void execute(const RenderScene &scene, const RenderPassContext &context, ShadowMapView shadows, const Mesh &plane);

  private:
    std::shared_ptr<Shader> dirShadowShader;
    std::shared_ptr<Shader> pointShadowShader;
};

class ForwardPass
{
  public:
    explicit ForwardPass(ResourceManager &resources);
    void restoreShaderBindings();
    void execute(const RenderScene &scene, const RenderPassContext &context, ShadowMapView shadows,
                 const FrameBuffer &output, const Mesh &plane);

  private:
    std::shared_ptr<Shader> modelShader;
};

class GBufferPass
{
  public:
    explicit GBufferPass(ResourceManager &resources);
    void execute(const RenderScene &scene, const RenderPassContext &context, const FrameBuffer &output,
                 const Mesh &plane);

  private:
    std::shared_ptr<Shader> gBufferShader;
};

class DeferredLightingPass
{
  public:
    explicit DeferredLightingPass(ResourceManager &resources);
    void restoreShaderBindings();
    void execute(const RenderScene &scene, const RenderPassContext &context, ShadowMapView shadows,
                 const FrameBuffer &gbuffer, const FrameBuffer &output, const Mesh &screenQuad);

  private:
    std::shared_ptr<Shader> lightPassShader;
};

class LightMarkerPass
{
  public:
    explicit LightMarkerPass(ResourceManager &resources);
    void execute(const RenderScene &scene, const RenderPassContext &context, const FrameBuffer &output,
                 const Mesh &cube);

  private:
    std::shared_ptr<Shader> lightShader;
};

class SkyboxPass
{
  public:
    explicit SkyboxPass(ResourceManager &resources);
    void restoreShaderBindings();
    void execute(const RenderPassContext &context, const FrameBuffer &output, const Mesh &cube, bool usePost);

  private:
    std::shared_ptr<Shader> backgroundShader;
};

class GBufferDebugPass
{
  public:
    explicit GBufferDebugPass(ResourceManager &resources);
    void execute(const RenderPassContext &context, const FrameBuffer &gbuffer, const FrameBuffer &output,
                 const Mesh &screenQuad);

  private:
    std::shared_ptr<Shader> gbufferDebugShader;
};

class BloomPass
{
  public:
    explicit BloomPass(ResourceManager &resources);
    void restoreShaderBindings();
    GLuint execute(const RenderPassContext &context, GLuint brightTexture,
                   const std::array<std::unique_ptr<FrameBuffer>, 2> &pingPong, const Mesh &screenQuad);

  private:
    std::shared_ptr<Shader> bloomBlurShader;
};

class ToneMappingPass
{
  public:
    explicit ToneMappingPass(ResourceManager &resources);
    void execute(const RenderPassContext &context, GLuint sceneTexture, GLuint bloomTexture, const FrameBuffer &output,
                 const Mesh &screenQuad);

  private:
    std::shared_ptr<Shader> sceneFramebufferShader;
};
} // namespace Rendering
