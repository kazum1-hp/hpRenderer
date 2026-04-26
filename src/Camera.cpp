#include "../include/Camera.h"

Camera::Camera()
    :cameraPos(0.0f, 0.0f, 5.0f),
     cameraFront(0.0f, 0.0f, -1.0f),
     cameraUp(0.0f, 1.0f, 0.0f),
     worldUp(0.0f, 1.0f, 0.0f),
     yaw(-90.f),
     pitch(0.0f),
     fov(45.0f),
     aspect(1920.0f / 1080.0f),
     nearPlane(0.1f),
     farPlane(1000.0f),
     sensitivity(0.04f)
{
    updateCameraVectors();
}

void Camera::updateCameraVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(cameraFront, worldUp));
    cameraUp = glm::normalize(glm::cross(right, cameraFront));
}

void Camera::ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (constrainPitch) {
        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;
    }

    updateCameraVectors();
}

void Camera::ProcessMouseScroll(float yoffset)
{
    fov -= yoffset;
    if (fov < 1.0f) fov = 1.0f;
    if (fov > 75.0f) fov = 75.0f;
}

void Camera::MoveForward(float speed)
{
    cameraPos += cameraFront * speed;
}

void Camera::MoveBackward(float speed)
{
    cameraPos -= cameraFront * speed;
}

void Camera::MoveLeft(float speed)
{
    cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
}

void Camera::MoveRight(float speed)
{
    cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
}

void Camera::MoveUp(float speed)
{
    cameraPos += cameraUp * speed;
}

void Camera::MoveDown(float speed)
{
    cameraPos -= cameraUp * speed;
}

void Camera::reset()
{
    cameraPos = { 0.0f, 0.0f, 5.0f };
    yaw = -90.0f;
    pitch = 0.0f;
    fov = 45.0f;
    updateCameraVectors();
}