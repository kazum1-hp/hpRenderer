#include "Editor/Panels.h"
#include "Scene.h"
#include "imgui.h"

void InspectorPanel::draw(Scene& scene)
{
    auto selectedObjectLabel = [&]() {
        const auto& objects = scene.GetObjects();
        if (objects.empty()) return std::string("No Object");

        const auto& object = objects[static_cast<size_t>(selectedIndex)];
        if (object.model && !object.model->getPath().empty())
        {
            return "Object " + std::to_string(selectedIndex);
        }

        return "Object " + std::to_string(selectedIndex);
    };

    auto& objects = scene.GetObjects();
    if (objects.empty())
    {
        selectedIndex = 0;
        ImGui::Text("No model objects in scene.");
    }
    else
    {
        if (selectedIndex < 0) selectedIndex = 0;
        if (selectedIndex >= static_cast<int>(objects.size()))
            selectedIndex = static_cast<int>(objects.size()) - 1;

        const std::string modelName = selectedObjectLabel();

        if (ImGui::BeginCombo("Select Object", modelName.c_str()))
        {
            for (int i = 0; i < static_cast<int>(objects.size()); ++i)
            {
                std::string label = "Object " + std::to_string(i);

                bool isSelected = (selectedIndex == i);
                if (ImGui::Selectable(label.c_str(), isSelected))
                {
                    selectedIndex = i;
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::Separator();
        ImGui::Text("Editing: %s", modelName.c_str());

        ImGui::PushID(selectedIndex);

        RenderObject& selectedObject = objects[static_cast<size_t>(selectedIndex)];
        ImGui::SliderFloat("aoBias", &selectedObject.material.aoBias, -1.0f, 1.0f);
        ImGui::SliderFloat("roughnessBias", &selectedObject.material.roughnessBias, -1.0f, 1.0f);
        ImGui::SliderFloat("metallicBias", &selectedObject.material.metallicBias, -1.0f, 1.0f);
        drawTransform(selectedObject.transform);
        ImGui::Checkbox("useNormal", &selectedObject.material.useNormalMap);

        ImGui::PopID();
    }

}

void InspectorPanel::drawTransform(Transform& transform)
{
    if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) return;
    ImGui::SeparatorText("Transform Settings");
    auto position = transform.getPosition();
    if (ImGui::DragFloat3("Position", &position[0], 0.1f))
        transform.setPosition(position);

    auto rotationDegrees = glm::degrees(transform.getRotation());
    if (ImGui::DragFloat3("Rotation", &rotationDegrees[0], 0.5f))
        transform.setRotation(glm::radians(rotationDegrees));

    auto scale = transform.getScale();
    if (ImGui::DragFloat3("Scale", &scale[0], 0.05f))
    {
        for (int i = 0; i < 3; ++i)
            if (scale[i] == 0.0f) scale[i] = 0.001f;
        transform.setScale(scale);
    }
    if (ImGui::Button("Reset")) transform.reset();
    ImGui::SameLine();
    if (ImGui::Button("Reset Rotation")) transform.resetRotation();
}
