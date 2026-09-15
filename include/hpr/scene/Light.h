#pragma once

#include "hpr/renderer/ibl/CubemapMath.h"

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

	glm::mat4 getPerspTransMatrix(unsigned int i) const {
		std::array<glm::mat4, 6> shadowTransforms = CalculateCubemapMatrices(position, near, far);

		return shadowTransforms[i];
	}
	float getFar() const { return far; }
	bool lightOn() const { return enabled; }

	void setEnabled(bool value) { enabled = value; }

private:
	LightType type;

	// light info
	glm::vec3 color;
	glm::vec3 position;
	glm::vec3 direction;
	float intensity;

	//projection matrix
	float near = 1.0f, far = 30.0f;
	float aspect = 1024.0f / 1024.0f;
	float speed = 0.2f;

	bool enabled = true;
};

