#include "hpr/editor/EditorLayer.h"
#include "imgui.h"

namespace
{
void Count(std::uint64_t value)
{
    if (value >= 1000000) ImGui::Text("%.2f M", static_cast<double>(value) / 1000000.0);
    else if (value >= 10000) ImGui::Text("%.2f K", static_cast<double>(value) / 1000.0);
    else ImGui::Text("%llu", static_cast<unsigned long long>(value));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%llu", static_cast<unsigned long long>(value));
}
void TimeRow(const char* label, double milliseconds, const char* description)
{
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(label);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", description);
    ImGui::TableNextColumn();
    if (milliseconds < 0) ImGui::TextDisabled("Pending");
    else ImGui::Text("%.3f ms", milliseconds);
}
}
void EditorLayer::drawStatistics(const Rendering::RendererStatistics& stats, double frameMs, double cpuFrameMs)
{
    if (ImGui::Begin("Renderer Statistics"))
    {
        if (ImGui::BeginTable("FrameStatistics", 2, ImGuiTableFlags_RowBg))
        {
            TimeRow("Frame Time", frameMs, "Previous application frame interval, including presentation and event polling.");
            TimeRow("CPU Frame Time", cpuFrameMs, "Previous frame: update, render submission and editor. Excludes swap buffers and event polling; includes driver stalls.");
            TimeRow("CPU Renderer", stats.cpuFrameMs, "Current Renderer::render wall time, including preparation and driver calls.");
            TimeRow("GPU Frame Time", stats.gpuValid ? stats.gpuQueries.front().milliseconds : -1,
                    "Latest completed renderer GPU timestamp interval. Excludes ImGui and presentation; can include GPU idle gaps between commands.");
            const auto countRow = [](const char* label, std::uint64_t count, const char* description) {
                ImGui::TableNextRow(); ImGui::TableNextColumn(); ImGui::TextUnformatted(label);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", description);
                ImGui::TableNextColumn(); Count(count);
            };
            countRow("Draw Calls", stats.drawCalls, "Current renderer frame, all passes and environment preparation. Excludes ImGui.");
            countRow("Triangles", stats.triangles, "Submitted input triangles, including repeated passes and instances. Excludes geometry-shader amplification; not visible triangles.");
            countRow("Rendered Objects", stats.renderedObjects, "Unique objects with nonempty camera-pass draws, including ground. Excludes shadow repeats, skybox, light markers and full-screen effects. Not a visibility query.");
            ImGui::EndTable();
        }
        ImGui::Separator();
        if (!stats.gpuSupported) ImGui::TextDisabled("GPU timestamps unavailable");
        else if (!stats.gpuValid) ImGui::TextDisabled("Waiting for GPU sample...");
        else
        {
            ImGui::Text("GPU sample: %llu frame(s) old", static_cast<unsigned long long>(stats.frameNumber - stats.gpuFrameNumber));
            if (ImGui::BeginTable("PassStatistics", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable))
            {
                ImGui::TableSetupColumn("Pass"); ImGui::TableSetupColumn("GPU ms");
                ImGui::TableSetupColumn("Draws"); ImGui::TableSetupColumn("Triangles");
                ImGui::TableHeadersRow();
                for (const auto& query : stats.gpuQueries)
                {
                    ImGui::TableNextRow(); ImGui::TableNextColumn();
                    if (query.depth) ImGui::Indent(query.depth * 8.0f);
                    ImGui::TextUnformatted(query.name.c_str());
                    if (query.depth) ImGui::Unindent(query.depth * 8.0f);
                    ImGui::TableNextColumn(); ImGui::Text("%.3f", query.milliseconds);
                    ImGui::TableNextColumn(); Count(query.drawCalls);
                    ImGui::TableNextColumn(); Count(query.triangles);
                }
                ImGui::EndTable();
            }
            ImGui::TextWrapped("Pass times and counts belong to the same GPU sample. Nested scopes include their children.");
        }
        if (stats.skippedGPUFrames)
            ImGui::Text("Skipped GPU samples: %llu", static_cast<unsigned long long>(stats.skippedGPUFrames));
    }
    ImGui::End();
}
