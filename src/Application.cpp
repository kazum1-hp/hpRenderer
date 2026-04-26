#include "../include/Application.h"
#include "../third_party/imgui/imgui.h"
#include "../third_party/imgui/backends/imgui_impl_glfw.h"
#include "../third_party/imgui/backends/imgui_impl_opengl3.h"

#include <iostream>

Application::Application(const char* title)
	: camera(),
	  input(camera),
	  window(title, input),
	  renderer(camera, input, window, mainScene),
	  running(true)
{
	if (!window.getWindow())
	{
		running = false;
		return;
	}

	init();
}

void Application::init()
{
	glEnable(GL_DEPTH_TEST);
	// If more initializations (such as framebuffers, post-processing systems) are needed in the future, they can be registered here.
	// For example:
	// renderer.initFrameBuffers();
	// sceneManager.loadDefaultScene();
	// ---------------------------------------------------------
	// Step A: Resource loading (only once)
	// Load model data into heap memory
	// ---------------------------------------------------------
	auto& res = ResourceManager::GetInstance();

	// Load Shader
	res.LoadShader("model", "../shaders/model.vs", "../shaders/model.fs");
    res.LoadShader("light", "../shaders/light.vs", "../shaders/light.fs");
    res.LoadShader("scene framebuffer", "../shaders/framebuffer.vs", "../shaders/framebuffer.fs");
    res.LoadShader("skybox", "../shaders/skybox.vs", "../shaders/skybox.fs");
    res.LoadShader("dir shadow", "../shaders/shadow.vs", "../shaders/shadow.fs");
    res.LoadShader("point shadow", "../shaders/pointShadow.vs", "../shaders/pointShadow.fs", "../shaders/pointShadow.gs");
    res.LoadShader("bloomBlur", "../shaders/bloomBlur.vs", "../shaders/bloomBlur.fs");
    res.LoadShader("gBuffer", "../shaders/gBuffer.vs", "../shaders/gBuffer.fs");;
    res.LoadShader("lightPass", "../shaders/lightPass.vs", "../shaders/lightPass.fs");
    res.LoadShader("debug", "../shaders/debug.vs", "../shaders/debug.fs");
    res.LoadShader("gbuffer debug", "../shaders/drawDebug.vs", "../shaders/drawDebug.fs");
	//res.LoadShader("pbr shader", "../shaders/pbr.vs", "../shaders/pbr.fs");
	res.LoadShader("background", "../shaders/background.vs", "../shaders/background.fs");
	res.LoadShader("irradiance", "../shaders/irradiance.vs", "../shaders/irradiance.fs");
	res.LoadShader("prefilter", "../shaders/prefilter.vs", "../shaders/prefilter.fs");
	res.LoadShader("brdf", "../shaders/brdf.vs", "../shaders/brdf.fs");

	auto model = res.LoadModel("../assets/models/blue_metal_plate_4k.gltf/blue_metal_plate_4k.gltf");
	auto material = res.LoadMaterial("material");
	auto environmentMap = res.LoadTexture("../assets/hdr/newman_cafeteria_4k.hdr", HDR);

	auto envAsset = std::make_shared<EnvironmentAsset>();
	envAsset->hdrTexture = environmentMap->getID();

	mainScene.AddObject(model, glm::vec3(0.0f, -0.5f, 0.0f), glm::vec3(0.3f), material);
	mainScene.AddPointLight(Light(glm::vec3(2.0f, 2.0f, 2.0f), 1.0f, glm::vec3(0.0f, 0.5f, 1.5f), LightType::Point));
	mainScene.AddPointLight(Light(glm::vec3(2.0f, 2.0f, 2.0f), 1.0f, glm::vec3(-4.0f, 0.5f, -3.0f), LightType::Point));
	mainScene.AddPointLight(Light(glm::vec3(2.0f, 2.0f, 2.0f), 1.0f, glm::vec3(3.0f, 0.5f, 1.0f), LightType::Point));
	mainScene.AddPointLight(Light(glm::vec3(2.0f, 2.0f, 2.0f), 1.0f, glm::vec3(-0.8f, 2.4f, -1.0f), LightType::Point));
	mainScene.SetEnvironment(envAsset);
	renderer.init();
}

void Application::initImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	// Enable docking and multi-viewport (allows windows to be dragged outside main window)
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // optional: keyboard controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // enable docking
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;     // enable multi-viewport / platform windows

	ImGui::StyleColorsDark();

	// When viewports enabled, tweak style for platform windows
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	ImGui_ImplGlfw_InitForOpenGL(window.getWindow(), true);
	ImGui_ImplOpenGL3_Init("#version 330");
}

void Application::shutdownImGui()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void Application::run()
{
	if (!running) return;

	// initial ImGui
	initImGui();

	float lastFrame = 0.0f;
	int drawCall = 1;

	while (!glfwWindowShouldClose(window.getWindow())) {
		
		
		float currentFrame = static_cast<float>(glfwGetTime());
		float deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		update(deltaTime);

		// Clear screen
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f); //custom color for screen clean
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderer.render(mainScene);
		//std::cout << "DrawCall: " << drawCall << std::endl;
		drawCall++;

		// ImGui new frame 
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		// ImGui content	
		renderer.onImGuiRender();

		// render ImGui
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Update and render additional platform windows when viewports are enabled
		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}

		// Swap buffers and poll IO events
		glfwSwapBuffers(window.getWindow());
		glfwPollEvents();
		
		if (input.shouldClose()) {
			glfwSetWindowShouldClose(window.getWindow(), true);
			continue; 
		}
	}
}

void Application::update(float deltaTime)
{
	input.update(window.getWindow(), deltaTime);
	// If SceneManager is to be added in the future, it can be updated here:
	// sceneManager.update(deltaTime);
}

void Application::shutdown()
{
	glfwTerminate();
}

Application::~Application()
{
	if (running)
	{
		shutdownImGui();
		shutdown();
	}
}