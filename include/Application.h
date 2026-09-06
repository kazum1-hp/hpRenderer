#pragma once
#include "Renderer.h"
#include "Camera.h"
#include "Window.h"
#include "InputManager.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "Editor/EditorLayer.h"

class Application
{
private:
	Camera camera;
	InputManager input;
	Scene mainScene;
	Window window;
	Renderer renderer;
	RenderSettings renderSettings;
	EditorLayer editor;
	bool running;

	void init();
	void update(float deltaTime);

public:
	Application(const char* title);
	void run();
	
	~Application();
};

