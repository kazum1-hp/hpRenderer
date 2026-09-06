#include "RenderExtraction.h"
#include "Scene.h"
#include "Camera.h"
#include <algorithm>
#include <stdexcept>

RenderScene BuildRenderScene(const Scene& scene)
{
    RenderScene result;
    result.objects.reserve(scene.GetObjects().size());
    for (const auto& object : scene.GetObjects())
    {
        result.objects.push_back({object.model, object.transform.getModelMatrix(), object.material});
    }
    const auto& directional = scene.GetDirLight();
    result.directionalLight = {directional.getColor(), directional.getLightDir(),
        directional.getIntensity(), directional.lightOn(),
        directional.getOrthoMatrix() * directional.getOrthoViewMatrix()};
    const auto count = std::min(scene.GetPointLights().size(), RenderLimits::MaxPointLights);
    result.pointLights.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const auto& light = scene.GetPointLights()[i];
        PointLightData data;
        data.color = light.getColor();
        data.position = light.getLightPos();
        data.intensity = light.getIntensity();
        data.farPlane = light.getFar();
        data.enabled = light.lightOn();
        for (unsigned int face = 0; face < 6; ++face)
            data.shadowMatrices[face] = light.getPerspTransMatrix(face);
        result.pointLights.push_back(data);
    }
    result.environment = scene.GetEnvironment().asset;
    return result;
}

CameraData BuildCameraData(const Camera& camera, RenderExtent targetExtent)
{
    if (!targetExtent.isValid()) throw std::invalid_argument("Camera extraction requires a valid render extent");
    CameraData result;
    result.view = camera.getViewMatrix();
    result.position = camera.getPosition();
    result.fovDegrees = camera.getFov();
    result.nearPlane = camera.getNearPlane();
    result.farPlane = camera.getFarPlane();
    const float aspect = static_cast<float>(targetExtent.width) / targetExtent.height;
    result.projection = glm::perspective(glm::radians(result.fovDegrees), aspect, result.nearPlane, result.farPlane);
    return result;
}
