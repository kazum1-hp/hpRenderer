#include "Editor/Panels.h"
#include "Scene.h"
#include "imgui.h"
#include <algorithm>

void LightingPanel::draw(Scene& scene)
{
    ImGui::Begin("Light Control");
    ImGui::Text("Point Lights");
    const auto count = std::min(scene.GetPointLights().size(), RenderLimits::MaxPointLights);
    for (std::size_t i = 0; i < count; ++i)
        drawPointLight(scene.GetPointLight(i), static_cast<int>(i));
    ImGui::Text("Dir Lights");
    ImGui::PushID("Directional");
    drawDirectionalLight(scene.GetDirLight());
    ImGui::PopID();
    ImGui::End();
}

void LightingPanel::drawDirectionalLight(Light& light)
{
    auto color = light.getColor();
    auto direction = light.getLightDir();
    auto intensity = light.getIntensity();
    if (ImGui::ColorEdit3("Light Color", &color[0])) light.setColor(color);
    if (ImGui::DragFloat3("Light Direction", &direction[0])) light.setLightDir(direction);
    if (ImGui::SliderFloat("Light Intensity", &intensity, 0.05f, 100.0f)) light.setIntensity(intensity);
}

void LightingPanel::drawPointLight(Light& light, int index)
{
    ImGui::PushID(index);
    auto enabled = light.lightOn();
    auto color = light.getColor();
    auto position = light.getLightPos();
    auto intensity = light.getIntensity();
    if (ImGui::Checkbox("Enabled", &enabled)) light.setEnabled(enabled);
    if (ImGui::ColorEdit3("Light Color", &color[0])) light.setColor(color);
    if (ImGui::DragFloat3("Light Position", &position[0])) light.setLightPos(position);
    if (ImGui::SliderFloat("Light Intensity", &intensity, 0.05f, 100.0f)) light.setIntensity(intensity);
    ImGui::PopID();
}
