#include "../include/Application.h"
#include "../include/RenderExtraction.h"

#include <iostream>

Application::Application(const char* title)
	: camera(),
	  input(camera),
	  mainScene(),
	  window(title, input),
	  renderer(),
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
	auto model2 = res.LoadModel("../assets/models/metal_office_desk_4k/metal_office_desk_4k.gltf");
	auto model3 = res.LoadModel("../assets/models/marble_bust_01_4k/marble_bust_01_4k.gltf");

	auto material = res.LoadMaterial("material");
	auto envAsset = res.LoadEnvironment("../assets/hdr/newport_loft.hdr");

	mainScene.AddObject(model, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.5f), material);
	mainScene.AddObject(model2, glm::vec3(0.0f, -5.5f, 0.0f), glm::vec3(5.0f), material);
	mainScene.AddObject(model3, glm::vec3(-3.0f, -1.5f, 0.0f), glm::vec3(5.0f), material);

	mainScene.AddPointLight(Light(glm::vec3(2.0f, 2.0f, 2.0f), 1.0f, glm::vec3(0.0f, 0.5f, 1.5f), LightType::Point));
	mainScene.AddPointLight(Light(glm::vec3(2.0f, 2.0f, 2.0f), 1.0f, glm::vec3(-4.0f, 0.5f, -3.0f), LightType::Point));
	mainScene.AddPointLight(Light(glm::vec3(2.0f, 2.0f, 2.0f), 1.0f, glm::vec3(3.0f, 0.5f, 1.0f), LightType::Point));
	mainScene.AddPointLight(Light(glm::vec3(2.0f, 2.0f, 2.0f), 1.0f, glm::vec3(-0.8f, 2.4f, -1.0f), LightType::Point));
	mainScene.SetEnvironment(envAsset);
	renderer.init(res, {static_cast<std::uint32_t>(window.getWidth()), static_cast<std::uint32_t>(window.getHeight())});
}

void Application::run()
{
	if (!running) return;

	// The editor owns its UI context and backends.
	editor.initialize(window.getWindow());

	float lastFrame = 0.0f;

	while (!glfwWindowShouldClose(window.getWindow())) {
		
		
		float currentFrame = static_cast<float>(glfwGetTime());
		float deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		update(deltaTime);

		// Clear screen
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f); //custom color for screen clean
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		const auto extent = renderer.resize(editor.requestedExtent());
		// Preserve the editor's sticky post-process toggle without mutating settings inside Renderer.
		if (renderSettings.deferred) renderSettings.postProcess.enabled = true;
		const RenderFrameData frame{currentFrame, input.isParallelLightOn(), input.isPointLightOn()};
		const auto output = renderer.render(BuildRenderScene(mainScene),
			BuildCameraData(camera, extent), renderSettings, frame);
		editor.beginFrame();
		editor.draw(mainScene, renderSettings, output, input,
			[this] { renderer.restoreShaderBindings(); });
		input.setCaptureState(editor.captureState());
		editor.endFrame();

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

Application::~Application()
{
	editor.shutdown();

	// Release every OpenGL-backed resource while the window/context still exists.
	renderer.shutdown();
	mainScene.Clear();
	ResourceManager::GetInstance().Clear();
}
