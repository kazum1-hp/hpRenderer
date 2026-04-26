#pragma once
#include <GLFW/glfw3.h>
#include "../third_party/imgui/imgui.h"
#include "Camera.h"

class Window;

class InputManager
{
public:
	InputManager(Camera& cam);

	void update(GLFWwindow* window, float deltaTime);
	bool shouldClose() const { return requestClose; }

	// GLFW callback hooks
	void onMouseMove(double xpos, double ypos);
	void onScroll(double xoffset, double yoffset);

	// Getter for states
	bool isCursorVisible() const { return cursorVisible; }
	bool isParallelLightOn() const { return parallelLightOn; }
	bool isPointLightOn() const { return pointLightOn; }
	//bool isSpotLightOn() const { return spotLightOn; }

	void onImGuiRender();
	void setSceneHovered(bool hovered) { sceneHovered = hovered; }

private:
	void handleCursor(GLFWwindow* window);
	void handleMovement(GLFWwindow* window, float deltaTime);
	void handleLightSwitch(GLFWwindow* window);
	void handleWindowClose(GLFWwindow* window);

private:
	Camera& camera;

	bool requestClose = false;

	bool parallelLightOn = false;
	bool pointLightOn = false;
	//bool spotLightOn = true;
	bool firstMouse = true;
	
	float lastX = 0.0f;
	float lastY = 0.0f;
	float moveSpeed = 5.0f;

	bool lastParallelKey = false;
	bool lastPointKey = false;
	bool lastSpotKey = false;

	bool cursorVisible = false;
	bool altPressed = false;
	bool mouseOverImGui = false;

	bool sceneHovered = false;
};

