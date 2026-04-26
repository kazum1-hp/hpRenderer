#pragma once

#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include "../third_party/imgui/imgui.h"

class Transform
{
private:
	glm::vec3 position{ 0.0f };
	glm::vec3 rotation{ 0.0f };
	glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

public:
    void setPosition(glm::vec3 pos) { position = pos; }
    void setRotation(glm::vec3 rot) { rotation = rot; }
    void setScale(glm::vec3 s) { scale = s; }

	glm::mat4 getModelMatrix() const
	{
		glm::mat4 model(1.0f);
		model = glm::translate(model, position);

		model = glm::rotate(model, rotation.x, glm::vec3(1, 0, 0));
		model = glm::rotate(model, rotation.y, glm::vec3(0, 1, 0));
		model = glm::rotate(model, rotation.z, glm::vec3(0, 0, 1));

		model = glm::scale(model, scale);

		return model;
	}

	void onImGuiRender()
	{
        if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SeparatorText("Transform Settings");

            // Position
            if (ImGui::DragFloat3("Position", &position[0], 0.1f))
            {
                //  dirty flag
            }

            // Rotation (in degrees for UI)
            glm::vec3 rotationDeg = glm::degrees(rotation);
            if (ImGui::DragFloat3("Rotation", &rotationDeg[0], 0.5f))
            {
                rotation = glm::radians(rotationDeg);
            }

            // Scale
            if (ImGui::DragFloat3("Scale", &scale[0], 0.05f))
            {
                // ensure scale not equal 0
                if (scale.x == 0) scale.x = 0.001f;
                if (scale.y == 0) scale.y = 0.001f;
                if (scale.z == 0) scale.z = 0.001f;
            }

            // Reset buttons
            if (ImGui::Button("Reset"))
            {
                position = glm::vec3(0.0f);
                rotation = glm::vec3(0.0f);
                scale = glm::vec3(1.0f);
            }

            ImGui::SameLine();
            if (ImGui::Button("Reset Rotation"))
            {
                rotation = glm::vec3(0.0f);
            }
        }
    }
};