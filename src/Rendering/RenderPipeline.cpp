#include "Rendering/RenderPipeline.h"
#include "Rendering/DrawHelpers.h"
#include "ResourceManager.h"
#include <iostream>

namespace Rendering
{
RenderPipeline::RenderPipeline(ResourceManager &resources, RenderExtent extent)
    : renderExtent(extent), screenQuad(resources.GetScreenQuad()), plane(resources.GetPlane()),
      cube(resources.GetCube()), shadow(resources), forward(resources), gbuffer(resources), lighting(resources),
      markers(resources), skybox(resources), debug(resources), bloom(resources), toneMapping(resources)
{
    targets.initialize(extent, ShadowSize, ColorFormat::RGBA16F);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
}

RenderOutput RenderPipeline::render(const RenderScene &scene, const CameraData &camera, const RenderSettings &settings,
                                    const RenderFrameData &frame, EnvironmentGpuView environment)
{
    const RenderPassContext context{camera,      settings, frame, environment, scene.directionalLight.lightSpaceMatrix,
                                    renderExtent};
    targets.syncPointShadows(PointLightCount(scene), ShadowSize);
    const ShadowMapView shadows{*targets.directionalShadow, targets.pointShadows};

    if (settings.shadows)
        shadow.execute(scene, context, shadows, *plane);

    const FrameBuffer &sceneTarget = settings.deferred ? *targets.deferredLighting : *targets.hdr;
    if (settings.deferred)
    {
        gbuffer.execute(scene, context, *targets.gbuffer, *plane);
        lighting.execute(scene, context, shadows, *targets.gbuffer, sceneTarget, *screenQuad);
    }
    else
    {
        forward.execute(scene, context, shadows, sceneTarget, *plane);
    }

    if (settings.drawLights)
        markers.execute(scene, context, sceneTarget, *cube);
    const bool usePost = settings.deferred || settings.postProcess.enabled;
    skybox.execute(context, sceneTarget, *cube, usePost);
    if (settings.deferred && settings.drawGBufferDebug)
        debug.execute(context, *targets.gbuffer, sceneTarget, *screenQuad);

    if (!usePost)
        return {sceneTarget.getColor(), renderExtent};

    GLuint bloomTexture = 0;
    if (settings.postProcess.bloom)
        bloomTexture = bloom.execute(context, sceneTarget.getColor(1), targets.bloomPingPong, *screenQuad);
    toneMapping.execute(context, sceneTarget.getColor(), bloomTexture, *targets.finalOutput, *screenQuad);
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
