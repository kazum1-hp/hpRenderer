#include "hpr/renderer/Renderer.h"
#include "hpr/renderer/opengl/Skybox.h"
#include "hpr/renderer/RenderPipeline.h"
#include <stdexcept>

Renderer::Renderer() = default;
Renderer::~Renderer() { shutdown(); }

void Renderer::init(AssetManager& resources, RenderExtent extent)
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
    sixFaceSkybox.reset();
    skyboxSource.reset();
    pipeline.reset();
}

RenderOutput Renderer::render(const RenderScene& scene, const CameraData& camera,
    const RenderSettings& settings, const RenderFrameData& frame)
{
    if (!pipeline) throw std::logic_error("Renderer::render called before initialization");
    EnvironmentGpuView environment;
    if (scene.environmentMode == EnvironmentMode::IBL) environment = iblCache.prepare(scene.environment);
    else if (scene.environmentMode == EnvironmentMode::SixFaces && scene.skybox)
    {
        if (skyboxSource.lock() != scene.skybox)
        {
            auto replacement = std::make_unique<Skybox>();
            if (replacement->load(*scene.skybox))
            {
                sixFaceSkybox = std::move(replacement);
                skyboxSource = scene.skybox;
            }
        }
        if (sixFaceSkybox) environment.envCubemap = sixFaceSkybox->getID();
    }
    return pipeline->render(scene, camera, settings, frame, environment);
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

