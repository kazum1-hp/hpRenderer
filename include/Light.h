#pragma once

#include "../third_party/imgui/imgui.h"

#include "ResourceManager.h"

enum class LightType {
	Directional,
	Point
};

class Light
{
public:
	//Light(glm::vec3 colour = glm::vec3(1.0f), glm::vec3 pos = glm::vec3(-6.0f, 6.0f, 6.0f), glm::vec3 dir = glm::vec3(-2.2f, -2.0f, -2.3f));
	Light(glm::vec3 color, float intensity, glm::vec3 dirOrPos, LightType type);

	void Update();

	void setColor(glm::vec3 colour) { color = colour; }
	void setLightPos(glm::vec3 pos) { position = pos; }
	void setLightDir(glm::vec3 dir) { direction = dir; }
	void setIntensity(float I) { intensity = I; }

	glm::vec3 getAmbient() const {
		return ambient * color;
	}
	glm::vec3 getDiffuse() const {
		return diffuse * color;
	}
	glm::vec3 getSpecular() const {
		return specular * color;
	}

	glm::vec3 getColor() const {
		return color;
	}
	glm::vec3 getLightPos() const {
		return position;
	}
	glm::vec3 getLightDir() const {
		return direction;
	}
	float getIntensity() const {
		return intensity;
	}

	glm::mat4 getOrthoViewMatrix() const {
		return glm::lookAt(-direction * distance, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	}
	glm::mat4 getOrthoMatrix() const {
		return glm::ortho(-orthoRange, orthoRange, -orthoRange, orthoRange, nearPlane, farPlane);
	}
	glm::mat4 getPerspTransMatrix(GLuint i) const {
		std::array<glm::mat4, 6> shadowTransforms = ResourceManager::calculateCubeMatrices(position, near, far);

		return shadowTransforms[i];
	}
	GLfloat getFar() const { return far; }
	bool lightOn() const { return enabled; }

	void dirOnImGuiRender();
	void pointOnImGuiRender(int index);

private:
	LightType type;

	// For bling-Phong brdf
	glm::vec3 ambient = glm::vec3(0.05f);
	glm::vec3 diffuse = glm::vec3(1.0f);
	glm::vec3 specular = glm::vec3(0.3f);

	// light info
	glm::vec3 color;
	glm::vec3 position;
	glm::vec3 direction;
	float intensity;

	//ortho matrix
	GLfloat orthoRange = 10.0f;
	GLfloat nearPlane = 1.0f, farPlane = 50.0f;
	float distance = 10.0f;

	//projection matrix
	GLfloat near = 1.0f, far = 30.0f;
	GLfloat aspect = 1024.0f / 1024.0f;
	float speed = 0.2f;

	bool enabled = true;
};

