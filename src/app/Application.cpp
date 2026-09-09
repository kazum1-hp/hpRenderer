#include "hpr/app/Application.h"
#include "hpr/renderer/RenderExtraction.h"
#include "hpr/assets/Model.h"

#include <iostream>
#include <stdexcept>

Application::Application(const char* title)
	: camera(),
	  input(camera),
	  window(title, input),
	  assets(),
	  mainScene(),
	  renderer(),
	  editor(assets),
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
	auto& res = assets;

	// Load Shader
	res.LoadShader(ShaderId::Model, "../shaders/model.vs", "../shaders/model.fs");
    res.LoadShader(ShaderId::Light, "../shaders/light.vs", "../shaders/light.fs");
    res.LoadShader(ShaderId::ToneMapping, "../shaders/framebuffer.vs", "../shaders/framebuffer.fs");
    res.LoadShader(ShaderId::EnvironmentCapture, "../shaders/skybox.vs", "../shaders/skybox.fs");
    res.LoadShader(ShaderId::DirectionalShadow, "../shaders/shadow.vs", "../shaders/shadow.fs");
    res.LoadShader(ShaderId::PointShadow, "../shaders/pointShadow.vs", "../shaders/pointShadow.fs", "../shaders/pointShadow.gs");
    res.LoadShader(ShaderId::BloomBlur, "../shaders/bloomBlur.vs", "../shaders/bloomBlur.fs");
    res.LoadShader(ShaderId::GBuffer, "../shaders/gBuffer.vs", "../shaders/gBuffer.fs");
    res.LoadShader(ShaderId::DeferredLighting, "../shaders/lightPass.vs", "../shaders/lightPass.fs");
    res.LoadShader(ShaderId::Debug, "../shaders/debug.vs", "../shaders/debug.fs");
    res.LoadShader(ShaderId::GBufferDebug, "../shaders/drawDebug.vs", "../shaders/drawDebug.fs");
	res.LoadShader(ShaderId::Skybox, "../shaders/background.vs", "../shaders/background.fs");
	res.LoadShader(ShaderId::Irradiance, "../shaders/irradiance.vs", "../shaders/irradiance.fs");
	res.LoadShader(ShaderId::Prefilter, "../shaders/prefilter.vs", "../shaders/prefilter.fs");
	res.LoadShader(ShaderId::Brdf, "../shaders/brdf.vs", "../shaders/brdf.fs");

	auto model3 = res.LoadModel("../assets/models/marble_bust_01_4k/marble_bust_01_4k.gltf");

	auto envAsset = res.LoadEnvironment("../assets/hdr/newport_loft.hdr");
    if (!model3 || !envAsset) throw std::runtime_error("Required startup model or HDR is missing");

#ifdef HPRENDERER_FULL_DEMO
    auto model = res.LoadModel("../assets/models/blue_metal_plate_4k.gltf/blue_metal_plate_4k.gltf");
    auto model2 = res.LoadModel("../assets/models/metal_office_desk_4k/metal_office_desk_4k.gltf");
    if (!model || !model2) throw std::runtime_error("Full demo model assets are missing");
    mainScene.AddObject(model, glm::vec3(0.0f), glm::vec3(0.5f));
    mainScene.AddObject(model2, glm::vec3(0.0f, -5.5f, 0.0f), glm::vec3(5.0f));
    mainScene.AddObject(model3, glm::vec3(-3.0f, -1.5f, 0.0f), glm::vec3(5.0f));
#else
    mainScene.AddObject(model3, glm::vec3(0.0f, -1.5f, 0.0f), glm::vec3(5.0f));
#endif

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
	assets.Clear();
}
