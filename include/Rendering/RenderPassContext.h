#pragma once
#include "RenderScene.h"
#include "RenderFrameData.h"
#include "RenderSettings.h"
#include "RenderTypes.h"
#include "IBLCache.h"
#include "FrameBuffer.h"
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
