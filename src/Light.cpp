#include "../include/Light.h"

Light::Light(glm::vec3 color, float intensity, glm::vec3 dirOrPos, LightType type)
	:color(color), intensity(intensity), type(type)
{
	if (type == LightType::Directional) {
		direction = dirOrPos;
		position = glm::vec3(0.0f);
	}
	else { // point light
		position = dirOrPos;
		direction = glm::vec3(0.0f); 
	}
}

void Light::Update()
{
	//float time = static_cast<float>(glfwGetTime());
	//float angle = time * speed;
	//direction = glm::normalize(glm::vec3(
	//	sin(angle),
	//	-1.0f,
	//	cos(angle)
	//));
}

void Light::dirOnImGuiRender()
{
	ImGui::ColorEdit3("Light Color", glm::value_ptr(color));
	ImGui::DragFloat3("Light Direction", glm::value_ptr(direction));
	ImGui::SliderFloat("Light Intensity", &intensity, 0.05f, 10.f);
}

void Light::pointOnImGuiRender(int index)
{
	ImGui::PushID(index);

	ImGui::Checkbox("Enabled", &enabled);
	ImGui::ColorEdit3("Light Color", glm::value_ptr(color));
	ImGui::DragFloat3("Light Position", glm::value_ptr(position));
	ImGui::SliderFloat("Light Intensity", &intensity, 0.05f, 10.f);

	ImGui::PopID();
}