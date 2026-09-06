#pragma once
#include "IBLCache.h"
#include "RenderSettings.h"
#include "RenderTypes.h"
#include "RenderScene.h"
#include "RenderFrameData.h"
#include <memory>

class ResourceManager;
namespace Rendering { class RenderPipeline; }

class Renderer
{
public:
    // Construction/destruction without init performs no GL calls.
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    // The caller owns/makes current the GL context for initialization, drawing and shutdown.
    void init(ResourceManager& resources, RenderExtent extent);
    void shutdown();
    RenderOutput render(const RenderScene& scene, const CameraData& camera,
        const RenderSettings& settings, const RenderFrameData& frame);
    // Returns the actual extent, including when a resize fails and retains old targets.
    RenderExtent resize(RenderExtent extent);
    void restoreShaderBindings();

private:
    std::unique_ptr<Rendering::RenderPipeline> pipeline;
    IBLCache iblCache;
};

