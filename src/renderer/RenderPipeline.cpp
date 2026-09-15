#include "hpr/renderer/RenderPipeline.h"
#include "hpr/renderer/passes/DrawHelpers.h"
#include "hpr/assets/AssetManager.h"
#include "hpr/renderer/opengl/PrimitiveMeshes.h"
#include "hpr/renderer/opengl/RenderProfiler.h"
#include "hpr/renderer/DirectionalShadow.h"
#include <iostream>

namespace Rendering
{
RenderPipeline::RenderPipeline(AssetManager &resources, RenderExtent extent)
    : models(resources), renderExtent(extent), screenQuad(CreateScreenQuad()), plane(CreatePlane({
        resources.LoadTexture("../assets/textures/bricks2/bricks2.jpg"),
        resources.LoadTexture("../assets/textures/bricks2/bricks2_normal.jpg", Normal),
        resources.LoadTexture("../assets/textures/bricks2/bricks2_disp.jpg", Height)})),
      cube(CreateCube()), shadow(resources), forward(resources), gbuffer(resources), lighting(resources),
      markers(resources), skybox(resources), debug(resources), bloom(resources), toneMapping(resources)
{
    targets.initialize(extent, ShadowSize, ColorFormat::RGBA16F);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

RenderOutput RenderPipeline::render(const RenderScene &source, const CameraData &camera, const RenderSettings &settings,
                                    const RenderFrameData &frame, EnvironmentGpuView environment)
{
    GpuRenderScene scene;
    {
        ScopedGPUQuery query("Scene Prepare");
        scene = models.prepare(source);
        targets.syncPointShadows(PointLightCount(scene), ShadowSize);
    }
    scene.directionalLight.direction = NormalizeLightDirection(scene.directionalLight.direction);
    DirectionalShadowProjection sunShadow;
    if (settings.shadows && frame.directionalLightEnabled && scene.directionalLight.enabled)
    {
        std::vector<Bounds> casters;
        casters.reserve(source.objects.size() + 1);
        for (const auto& object : source.objects)
            if (object.model)
                casters.push_back(object.model->getBounds().transformed(object.transform));
        if (settings.groundPlane.visible)
            casters.push_back({glm::vec3(-25, -5.5f, -25), glm::vec3(25, -5.5f, 25)});
        sunShadow = BuildDirectionalShadowProjection(camera, scene.directionalLight.direction,
            settings.directionalShadowDistance, ShadowSize, casters);
    }
    const RenderPassContext context{camera, settings, frame, environment, sunShadow.matrix,
                                    renderExtent, sunShadow.depthRange};
    const ShadowMapView shadows{*targets.directionalShadow, targets.pointShadows};

    if (settings.shadows)
    {
        ScopedGPUQuery query("Shadow Pass");
        shadow.execute(scene, context, shadows, *plane);
    }

    const FrameBuffer &sceneTarget = settings.deferred ? *targets.deferredLighting : *targets.hdr;
    if (settings.deferred)
    {
        {
            ScopedGPUQuery query("Geometry Pass");
            gbuffer.execute(scene, context, *targets.gbuffer, *plane);
        }
        ScopedGPUQuery query("Lighting Pass");
        lighting.execute(scene, context, shadows, *targets.gbuffer, sceneTarget, *screenQuad);
    }
    else
    {
        ScopedGPUQuery query("Forward Pass");
        forward.execute(scene, context, shadows, sceneTarget, *plane);
    }

    if (settings.drawLights)
    {
        ScopedGPUQuery query("Light Markers");
        markers.execute(scene, context, sceneTarget, *cube);
    }
    {
        ScopedGPUQuery query("Skybox");
        skybox.execute(context, sceneTarget, *cube);
    }
    // Both paths composite blended materials forward, after opaque depth and the skybox.
    {
        ScopedGPUQuery query("Transparent Pass");
        forward.execute(scene, context, shadows, sceneTarget, *plane, ForwardPhase::Transparent);
    }
    if (settings.deferred && settings.drawGBufferDebug)
    {
        ScopedGPUQuery query("GBuffer Debug");
        debug.execute(context, *targets.gbuffer, sceneTarget, *screenQuad);
    }

    GLuint bloomTexture = 0;
    if (settings.postProcess.enabled && settings.postProcess.bloom)
    {
        ScopedGPUQuery query("Bloom");
        bloomTexture = bloom.execute(context, sceneTarget.getColor(1), targets.bloomPingPong, *screenQuad);
    }
    {
        ScopedGPUQuery query("Tone Mapping");
        toneMapping.execute(context, sceneTarget.getColor(), bloomTexture, *targets.finalOutput, *screenQuad);
    }
    return {targets.finalOutput->getColor(), renderExtent};
}

RenderExtent RenderPipeline::resize(RenderExtent extent)
{
    if (!extent.isValid() || (extent.width == renderExtent.width && extent.height == renderExtent.height))
        return renderExtent;
    try
    {
        targets.resizeViewport(extent);
    }
    catch (const std::exception &error)
    {
        std::cerr << "Viewport resize failed: " << error.what() << std::endl;
        return renderExtent;
    }
    renderExtent = extent;
    return renderExtent;
}

void RenderPipeline::restoreShaderBindings()
{
    forward.restoreShaderBindings();
    lighting.restoreShaderBindings();
    skybox.restoreShaderBindings();
    bloom.restoreShaderBindings();
}
} // namespace Rendering
