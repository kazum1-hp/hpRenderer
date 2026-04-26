#include <iostream>
#include "../include/InputManager.h"
#include "../third_party/imgui/imgui.h"

InputManager::InputManager(Camera& cam)
	:camera(cam) {
}

void InputManager::update(GLFWwindow* window, float deltaTime)
{
	handleCursor(window);
	handleMovement(window, deltaTime);
	handleLightSwitch(window);
	handleWindowClose(window);
}

void InputManager::onMouseMove(double xpos, double ypos)
{
	if (cursorVisible)
		return;

	if (firstMouse) {
		lastX = static_cast<float>(xpos);
		lastY = static_cast<float>(ypos);
		firstMouse = false;
	}

	float xoffset = static_cast<float>(xpos - lastX);
	float yoffset = static_cast<float>(lastY - ypos); // Reverse Y
	lastX = static_cast<float>(xpos);
	lastY = static_cast<float>(ypos);

	camera.ProcessMouseMovement(xoffset, yoffset);
}

void InputManager::onScroll(double xoffset, double yoffset)
{
	if (!sceneHovered)
		return;

	camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

void InputManager::handleCursor(GLFWwindow* window)
{
	ImGuiIO& io = ImGui::GetIO();
	mouseOverImGui = io.WantCaptureMouse && !sceneHovered;

	bool altNow = glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS;
	if (altNow != altPressed)
		altPressed = altNow;

	if (altPressed || mouseOverImGui) {
		if (!cursorVisible) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			cursorVisible = true;
		}
	}
	else {
		if (cursorVisible) {
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			cursorVisible = false;
			firstMouse = true; // Reset to avoid jumping
		}
	}
}

void InputManager::handleMovement(GLFWwindow* window, float deltaTime)
{
	float speed = moveSpeed * deltaTime;

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.MoveForward(speed);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.MoveBackward(speed);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.MoveLeft(speed);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.MoveRight(speed);
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
		camera.MoveUp(speed);
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
		camera.MoveDown(speed);

	if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
		camera.reset();
}

void InputManager::handleLightSwitch(GLFWwindow* window)
{
	bool parallelPressed = glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS;
	bool pointPressed = glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS;
	bool spotPressed = glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS;

	if (parallelPressed && !lastParallelKey)
		parallelLightOn = !parallelLightOn;
	if (pointPressed && !lastPointKey)
		pointLightOn = !pointLightOn;
	/*if (spotPressed && !lastSpotKey)
		spotLightOn = !spotLightOn;*/

	lastParallelKey = parallelPressed;
	lastPointKey = pointPressed;
	lastSpotKey = spotPressed;
}

void InputManager::handleWindowClose(GLFWwindow* window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		requestClose = true;
}

void InputManager::onImGuiRender()
{
	ImGui::Text("Input Settings");
	ImGui::SliderFloat("Move Speed", &moveSpeed, 1.0f, 100.0f);
}