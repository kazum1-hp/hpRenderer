#pragma once
#include "glm/glm.hpp"
#include <string.h>

class Material {
private:
	std::string name;
    glm::vec3 ambient;
    glm::vec3 diffuse;
    glm::vec3 specular;
    float shininess;

public:
	Material(const std::string& name)
		:ambient(1.0f, 0.5f, 0.31f),
		 diffuse(1.0f, 0.5f, 0.31f),
		 specular(0.5f, 0.5f, 0.5f),
		 shininess(32.0f) {}

	glm::vec3 getAmbient() const {
		return ambient;
	}
	glm::vec3 getDiffuse() const {
		return diffuse;
	}
	glm::vec3 getSpecular() const {
		return specular;
	}
	float getShininess() const {
		return shininess;
	}

	void setAmbient(glm::vec3 amb) { ambient = amb; }
	void setDiffuse(glm::vec3 dif) { diffuse = dif; }
	void setSpecular(glm::vec3 spec) { specular = spec; }
	void setShininess(float shin) { shininess = shin; }
};

