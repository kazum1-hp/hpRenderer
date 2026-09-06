#include "Renderer.h"
#include "Rendering/RenderPipeline.h"
#include <stdexcept>

Renderer::Renderer() = default;
Renderer::~Renderer() { shutdown(); }

void Renderer::init(ResourceManager& resources, RenderExtent extent)
{
    if (!glGetString || !glGetString(GL_VERSION))
        throw std::logic_error("Renderer::init requires a current OpenGL context");
    shutdown();
    try
    {
        pipeline = std::make_unique<Rendering::RenderPipeline>(resources, extent);
        iblCache.initialize(resources);
        restoreShaderBindings();
    }
    catch (...) { shutdown(); throw; }
}

void Renderer::shutdown()
{
    iblCache.clear();
    pipeline.reset();
}

RenderOutput Renderer::render(const RenderScene& scene, const CameraData& camera,
    const RenderSettings& settings, const RenderFrameData& frame)
{
    if (!pipeline) throw std::logic_error("Renderer::render called before initialization");
    return pipeline->render(scene, camera, settings, frame, iblCache.prepare(scene.environment));
}

RenderExtent Renderer::resize(RenderExtent extent)
{
    if (!pipeline) throw std::logic_error("Renderer::resize called before initialization");
    return pipeline->resize(extent);
}

void Renderer::restoreShaderBindings()
{
    if (pipeline) pipeline->restoreShaderBindings();
}

