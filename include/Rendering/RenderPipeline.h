#pragma once
#include "Rendering/RenderPasses.h"
#include "RenderTargets.h"

namespace Rendering
{
// Owns viewport targets and pass ordering, not Scene/editor/application state.
class RenderPipeline
{
  public:
    RenderPipeline(ResourceManager &resources, RenderExtent extent);
    RenderOutput render(const RenderScene &scene, const CameraData &camera, const RenderSettings &settings,
                        const RenderFrameData &frame, EnvironmentGpuView environment);
    RenderExtent resize(RenderExtent extent);
    void restoreShaderBindings();

  private:
    static constexpr unsigned int ShadowSize = 1024;
    RenderTargets targets;
    RenderExtent renderExtent;
    std::shared_ptr<Mesh> screenQuad, plane, cube;
    ShadowPass shadow;
    ForwardPass forward;
    GBufferPass gbuffer;
    DeferredLightingPass lighting;
    LightMarkerPass markers;
    SkyboxPass skybox;
    GBufferDebugPass debug;
    BloomPass bloom;
    ToneMappingPass toneMapping;
};
} // namespace Rendering
