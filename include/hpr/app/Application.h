#pragma once
#include "hpr/renderer/Renderer.h"
#include "hpr/scene/Camera.h"
#include "hpr/core/Window.h"
#include "hpr/core/InputManager.h"
#include "hpr/assets/AssetManager.h"
#include "hpr/scene/Scene.h"
#include "hpr/editor/EditorLayer.h"

class Application
{
private:
	Camera camera;
	InputManager input;
	Window window;
	AssetManager assets;
	Scene mainScene;
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

