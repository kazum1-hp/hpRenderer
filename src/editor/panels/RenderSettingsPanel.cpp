#include "hpr/editor/panels/Panels.h"
#include "hpr/renderer/RenderSettings.h"
#include "hpr/core/InputManager.h"
#include "imgui.h"

void RenderSettingsPanel::draw(RenderSettings& settings, InputManager& input)
{
    ImGui::Text("Input Settings");
    float moveSpeed = input.getMoveSpeed();
    if (ImGui::SliderFloat("Move Speed", &moveSpeed, 1.0f, 100.0f))
        input.setMoveSpeed(moveSpeed);

    ImGui::Checkbox("useShadow", &settings.shadows);
    ImGui::SameLine();
    ImGui::Checkbox("drawLights", &settings.drawLights);
    if (settings.shadows)
    {
        ImGui::SliderFloat("Sun Shadow Distance", &settings.directionalShadowDistance, 1.0f, 1000.0f, "%.1f");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Camera-forward coverage in world units. Shadows fade over the last 20%%.\nLarger distances cover more of the scene with less detail.");
    }

    ImGui::Checkbox("drawPlane", &settings.groundPlane.visible);

    if (settings.groundPlane.visible)
    {
        ImGui::SameLine();
        ImGui::Checkbox("useNormal", &settings.groundPlane.useNormalMap);

        ImGui::SameLine();

        ImGui::Checkbox("useHeight", &settings.groundPlane.useHeightMap);
        if (settings.groundPlane.useHeightMap)
        {
            ImGui::SliderFloat("height_scale", &settings.groundPlane.heightScale, 0.0001f, 0.01f);
        }
    }

    ImGui::Checkbox("useDeferred", &settings.deferred);
    if (settings.deferred)
    {
        ImGui::SameLine();
        ImGui::Checkbox("drawDebug", &settings.drawGBufferDebug);
    }
}

void RenderSettingsPanel::drawPostProcessing(RenderSettings& settings, RenderExtent renderExtent)
{
    // --------------------- Post Processing --------------------------
    ImGui::Begin("Post Processing");
    ImGui::Checkbox("usePost", &settings.postProcess.enabled);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Enable custom tone mapping, exposure and effects.\nWhen off: default Reinhard + gamma output remains active.");

    if (settings.postProcess.enabled)
    {
        ImGui::Checkbox("useHdr", &settings.postProcess.hdr);
        if (settings.postProcess.hdr)
        {
            ImGui::Combo("ToneMapping Mode", &settings.postProcess.toneMappingMode, "reinhard\0simple exposure\0ACESFilm\0Hable\0\0");
            ImGui::SliderFloat("Exposure", &settings.postProcess.exposure, 0.01f, 10.0f);
        }

        if (settings.drawLights)
        {
            ImGui::Checkbox("useBloom", &settings.postProcess.bloom);
            if (settings.postProcess.bloom)
                ImGui::SliderFloat("samplerDistance", &settings.postProcess.bloomSampleDistance, 0.01f, 10.0f);
        }

        ImGui::Combo("Effect Mode", &settings.postProcess.effectMode, "normal\0inversion\0grayscale\0sharpen\0blur\0\0");
        if (settings.postProcess.effectMode == 3 || settings.postProcess.effectMode == 4)
        {
            ImGui::SliderFloat("Offset", &settings.postProcess.kernelOffset, 100.0f, 1000.0f);
        }
        ImGui::SliderFloat("Scan Pos", &settings.postProcess.scanPosition, 0.0f, static_cast<float>(renderExtent.width));
    }

    ImGui::End();

}
