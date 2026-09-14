#include "hpr/core/Window.h"
#include <iostream>
#include <stdexcept>


Window::Window(const char* title, InputManager& inputManager, int width, int height)
	: input(inputManager), SCR_WIDTH(width), SCR_HEIGHT(height)
{
	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize GLFW; check your Windows graphics driver");
	}

	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, title, NULL, NULL);
	if (window == NULL) {
		glfwTerminate();
		throw std::runtime_error("Cannot create an OpenGL 3.3 window; install a compatible graphics driver");
	}

	// get primary monitor and set init window pos
	GLFWmonitor* primary = glfwGetPrimaryMonitor();
	const GLFWvidmode* mode = primary ? glfwGetVideoMode(primary) : nullptr;
	if (mode)
		glfwSetWindowPos(window, (mode->width - SCR_WIDTH) / 2, (mode->height - SCR_HEIGHT) / 2);

	glfwMakeContextCurrent(window);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		glfwDestroyWindow(window);
		window = nullptr;
		glfwTerminate();
		throw std::runtime_error("Failed to load OpenGL functions");
	}

	glViewport(0, 0, static_cast<GLsizei>(SCR_WIDTH), static_cast<GLsizei>(SCR_HEIGHT));
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
	glfwSetScrollCallback(window, scroll_callback);
}

Window::~Window()
{
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Window::framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	if (width == 0 || height == 0) {
		return;
	}

	glViewport(0, 0, width, height);

	Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
	if (win)
	{
		win -> SCR_WIDTH = width;
		win -> SCR_HEIGHT = height;

		std::cout << "Framebuffer resized: " << width << " x " << height << std::endl;

		if (win->onFramebufferResize) {
			win->onFramebufferResize(width, height);
		}
	}
}

void Window::cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
	Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
	if (win)
		win->getInput().onMouseMove(xpos, ypos);
}

void Window::scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	Window* win = static_cast<Window*>(glfwGetWindowUserPointer(window));
	if (win)
		win->getInput().onScroll(xoffset, yoffset);
}
