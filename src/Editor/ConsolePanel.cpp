#include "Editor/Panels.h"
#include "imgui.h"

void ConsolePanel::draw()
{
    ImGui::Begin("Console");
    if (ImGui::Button("Clear")) capture.clear();
    const auto output = capture.text();
    ImGui::TextUnformatted(output.c_str());
    ImGui::End();
}
