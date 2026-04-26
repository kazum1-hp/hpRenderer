#pragma once
#include "Renderer.h"
#include "Camera.h"
#include "Window.h"
#include "InputManager.h"
#include "ResourceManager.h"
#include "Scene.h"

class Application
{
private:
	Camera camera;
	Window window;
	Renderer renderer;
	InputManager input;
	Scene mainScene;
	bool running;

	void init();
	void initImGui();
	void shutdownImGui();
	void shutdown();
	void update(float deltaTime);

public:
	Application(const char* title);
	void run();
	
	~Application();
};

