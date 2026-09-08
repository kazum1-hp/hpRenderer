#pragma once
#include "hpr/renderer/ibl/IBLCache.h"
#include "hpr/renderer/RenderSettings.h"
#include "hpr/renderer/RenderTypes.h"
#include "hpr/renderer/RenderScene.h"
#include "hpr/renderer/RenderFrameData.h"
#include <memory>

class AssetManager;
class Skybox;
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
    void init(AssetManager& resources, RenderExtent extent);
    void shutdown();
    RenderOutput render(const RenderScene& scene, const CameraData& camera,
        const RenderSettings& settings, const RenderFrameData& frame);
    // Returns the actual extent, including when a resize fails and retains old targets.
    RenderExtent resize(RenderExtent extent);
    void restoreShaderBindings();

private:
    std::unique_ptr<Rendering::RenderPipeline> pipeline;
    IBLCache iblCache;
    std::unique_ptr<Skybox> sixFaceSkybox;
    std::weak_ptr<const SkyboxAsset> skyboxSource;
};

