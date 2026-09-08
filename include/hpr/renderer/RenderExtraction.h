#pragma once
#include "hpr/renderer/RenderScene.h"
#include "hpr/renderer/RenderTypes.h"
#include "hpr/renderer/RenderFrameData.h"

class Scene;
class Camera;

// Host-side adapters: Renderer never retains or accesses the source Scene/Camera.
RenderScene BuildRenderScene(const Scene& scene);
CameraData BuildCameraData(const Camera& camera, RenderExtent targetExtent);
