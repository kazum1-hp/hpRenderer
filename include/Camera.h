#pragma once
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"

class Camera
{
public:
    Camera();
    
    float aspect;

    void MoveForward(float speed);
    void MoveBackward(float speed);
    void MoveLeft(float speed);
    void MoveRight(float speed);
    void MoveUp(float speed);
    void MoveDown(float speed);

    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);
    void ProcessMouseScroll(float yoffset);

    void setSensitivity(float s) { sensitivity = s; }
    void reset();

    glm::vec3 getPosition() const { return cameraPos; }
    glm::vec3 getFront() const { return cameraFront; }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
    }
    glm::mat4 getProjectionMatrix() const {
        return glm::perspective(glm::radians(fov), aspect, nearPlane, farPlane);
    }

    float getFov() const { return fov; }
    float getNearPlane() const { return nearPlane; }
    float getFarPlane() const { return farPlane; }

private:
    void updateCameraVectors();

private:
    glm::vec3 cameraPos;
    glm::vec3 cameraFront;
    glm::vec3 cameraUp;
    glm::vec3 worldUp;

    float yaw;
    float pitch;

    float fov;
    float nearPlane;
    float farPlane;

    float sensitivity;
};
