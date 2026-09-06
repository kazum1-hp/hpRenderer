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
