#pragma once
#include "hpr/renderer/passes/RenderPassContext.h"
#include "hpr/renderer/opengl/Shader.h"
#include <array>

class AssetManager;
class Mesh;

namespace Rendering
{
enum class ForwardPhase { Opaque, Transparent };
// Concrete passes: each owns its shader references and receives explicit targets.
// execute() establishes its framebuffer/viewport/depth state; inputs are never retained.
class ShadowPass
{
  public:
    explicit ShadowPass(AssetManager &resources);
    void execute(const GpuRenderScene &scene, const RenderPassContext &context, ShadowMapView shadows, const Mesh &plane);

  private:
    std::shared_ptr<Shader> dirShadowShader;
    std::shared_ptr<Shader> pointShadowShader;
};

class ForwardPass
{
  public:
    explicit ForwardPass(AssetManager &resources);
    void restoreShaderBindings();
    void execute(const GpuRenderScene &scene, const RenderPassContext &context, ShadowMapView shadows,
                 const FrameBuffer &output, const Mesh &plane, ForwardPhase phase = ForwardPhase::Opaque);

  private:
    std::shared_ptr<Shader> modelShader;
};

class GBufferPass
{
  public:
    explicit GBufferPass(AssetManager &resources);
    void execute(const GpuRenderScene &scene, const RenderPassContext &context, const FrameBuffer &output,
                 const Mesh &plane);

  private:
    std::shared_ptr<Shader> gBufferShader;
};

class DeferredLightingPass
{
  public:
    explicit DeferredLightingPass(AssetManager &resources);
    void restoreShaderBindings();
    void execute(const GpuRenderScene &scene, const RenderPassContext &context, ShadowMapView shadows,
                 const FrameBuffer &gbuffer, const FrameBuffer &output, const Mesh &screenQuad);

  private:
    std::shared_ptr<Shader> lightPassShader;
};

class LightMarkerPass
{
  public:
    explicit LightMarkerPass(AssetManager &resources);
    void execute(const GpuRenderScene &scene, const RenderPassContext &context, const FrameBuffer &output,
                 const Mesh &cube);

  private:
    std::shared_ptr<Shader> lightShader;
};

class SkyboxPass
{
  public:
    explicit SkyboxPass(AssetManager &resources);
    void restoreShaderBindings();
    void execute(const RenderPassContext &context, const FrameBuffer &output, const Mesh &cube);

  private:
    std::shared_ptr<Shader> backgroundShader;
};

class GBufferDebugPass
{
  public:
    explicit GBufferDebugPass(AssetManager &resources);
    void execute(const RenderPassContext &context, const FrameBuffer &gbuffer, const FrameBuffer &output,
                 const Mesh &screenQuad);

  private:
    std::shared_ptr<Shader> gbufferDebugShader;
};

class BloomPass
{
  public:
    explicit BloomPass(AssetManager &resources);
    void restoreShaderBindings();
    GLuint execute(const RenderPassContext &context, GLuint brightTexture,
                   const std::array<std::unique_ptr<FrameBuffer>, 2> &pingPong, const Mesh &screenQuad);

  private:
    std::shared_ptr<Shader> bloomBlurShader;
};

class ToneMappingPass
{
  public:
    explicit ToneMappingPass(AssetManager &resources);
    void execute(const RenderPassContext &context, GLuint sceneTexture, GLuint bloomTexture, const FrameBuffer &output,
                 const Mesh &screenQuad);

  private:
    std::shared_ptr<Shader> sceneFramebufferShader;
};
} // namespace Rendering
