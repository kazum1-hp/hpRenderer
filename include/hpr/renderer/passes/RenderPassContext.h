#pragma once
#include "hpr/renderer/RenderScene.h"
#include "hpr/renderer/RenderFrameData.h"
#include "hpr/renderer/RenderSettings.h"
#include "hpr/renderer/RenderTypes.h"
#include "hpr/renderer/ibl/IBLCache.h"
#include "hpr/renderer/opengl/FrameBuffer.h"
#include <memory>
#include <vector>

namespace Rendering
{
// Borrowed inputs valid only for the current synchronous submission.
struct RenderPassContext
{
    const CameraData &camera;
    const RenderSettings &settings;
    const RenderFrameData &frame;
    EnvironmentGpuView environment;
    glm::mat4 lightSpaceMatrix;
    RenderExtent extent;
};

struct ShadowMapView
{
    const FrameBuffer &directional;
    const std::vector<std::unique_ptr<FrameBuffer>> &points;
};
} // namespace Rendering
