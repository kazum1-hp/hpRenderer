#pragma once

#include "glm/glm.hpp"
#include <glm/gtc/matrix_transform.hpp>

class Transform
{
private:
	glm::vec3 position{ 0.0f };
	glm::vec3 rotation{ 0.0f };
	glm::vec3 scale{ 1.0f, 1.0f, 1.0f };
    glm::vec3 initialPosition{ 0.0f };
    glm::vec3 initialScale{ 1.0f, 1.0f, 1.0f };

public:
    void setPosition(glm::vec3 pos) { position = pos; }
    void setRotation(glm::vec3 rot) { rotation = rot; }
    void setScale(glm::vec3 s) { scale = s; }
    void setInitialTransform(glm::vec3 pos, glm::vec3 s)
    {
        initialPosition = pos;
        initialScale = s;
        position = pos;
        scale = s;
    }

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

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getRotation() const { return rotation; }
    glm::vec3 getScale() const { return scale; }

    void reset()
    {
        position = initialPosition;
        rotation = glm::vec3(0.0f);
        scale = initialScale;
    }
    void resetRotation() { rotation = glm::vec3(0.0f); }
};
