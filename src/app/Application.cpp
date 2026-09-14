#include "hpr/app/Application.h"
#include "hpr/renderer/RenderExtraction.h"
#include "hpr/assets/Model.h"
#include "hpr/core/RuntimePaths.h"

#include <iostream>
#include <stdexcept>
#include <chrono>

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
    const auto shaderDirectory = ResolveShaderDirectory(std::filesystem::current_path());
    std::cout << "Shader directory: " << shaderDirectory.u8string() << std::endl;
    const auto shaderPath = [&](const char* name) { return (shaderDirectory / name).u8string(); };
	res.LoadShader(ShaderId::Model, shaderPath("model.vs"), shaderPath("model.fs"));
    res.LoadShader(ShaderId::Light, shaderPath("light.vs"), shaderPath("light.fs"));
    res.LoadShader(ShaderId::ToneMapping, shaderPath("framebuffer.vs"), shaderPath("framebuffer.fs"));
    res.LoadShader(ShaderId::EnvironmentCapture, shaderPath("skybox.vs"), shaderPath("skybox.fs"));
    res.LoadShader(ShaderId::DirectionalShadow, shaderPath("shadow.vs"), shaderPath("shadow.fs"));
    res.LoadShader(ShaderId::PointShadow, shaderPath("pointShadow.vs"), shaderPath("pointShadow.fs"), shaderPath("pointShadow.gs"));
    res.LoadShader(ShaderId::BloomBlur, shaderPath("bloomBlur.vs"), shaderPath("bloomBlur.fs"));
    res.LoadShader(ShaderId::GBuffer, shaderPath("gBuffer.vs"), shaderPath("gBuffer.fs"));
    res.LoadShader(ShaderId::DeferredLighting, shaderPath("lightPass.vs"), shaderPath("lightPass.fs"));
    res.LoadShader(ShaderId::Debug, shaderPath("debug.vs"), shaderPath("debug.fs"));
    res.LoadShader(ShaderId::GBufferDebug, shaderPath("drawDebug.vs"), shaderPath("drawDebug.fs"));
	res.LoadShader(ShaderId::Skybox, shaderPath("background.vs"), shaderPath("background.fs"));
	res.LoadShader(ShaderId::Irradiance, shaderPath("irradiance.vs"), shaderPath("irradiance.fs"));
	res.LoadShader(ShaderId::Prefilter, shaderPath("prefilter.vs"), shaderPath("prefilter.fs"));
	res.LoadShader(ShaderId::Brdf, shaderPath("brdf.vs"), shaderPath("brdf.fs"));

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

    using FrameClock = std::chrono::steady_clock;
    auto previousStart = FrameClock::now();
    bool hasPreviousFrame = false;
    double previousCpuMs = -1.0;

	while (!glfwWindowShouldClose(window.getWindow())) {
		
		
        const auto frameStart = FrameClock::now();
        const double frameMs = hasPreviousFrame
            ? std::chrono::duration<double, std::milli>(frameStart - previousStart).count() : -1.0;
        previousStart = frameStart;
        hasPreviousFrame = true;
        const float currentFrame = static_cast<float>(glfwGetTime());
        const float deltaTime = frameMs >= 0 ? static_cast<float>(frameMs / 1000.0) : 0.0f;
		update(deltaTime);

		// Clear screen
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f); //custom color for screen clean
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		const auto extent = renderer.resize(editor.requestedExtent());
		const RenderFrameData frame{currentFrame, input.isParallelLightOn(), input.isPointLightOn()};
		const auto output = renderer.render(BuildRenderScene(mainScene),
			BuildCameraData(camera, extent), renderSettings, frame);
		editor.beginFrame(!input.isCursorVisible());
		editor.draw(mainScene, renderSettings, output, input,
			[this] { renderer.restoreShaderBindings(); });
        editor.drawStatistics(renderer.statistics(), frameMs, previousCpuMs);
		input.setCaptureState(editor.captureState());
		editor.endFrame();
        previousCpuMs = std::chrono::duration<double, std::milli>(FrameClock::now() - frameStart).count();

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
