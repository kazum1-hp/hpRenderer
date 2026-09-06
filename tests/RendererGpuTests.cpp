#include "Renderer.h"
#include "RenderExtraction.h"
#include "ResourceManager.h"
#include "Scene.h"
#include "Camera.h"
#include "Rendering/RenderPasses.h"
#include "RenderTargets.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }

    int uniformInt(const Shader& shader, const char* name)
    {
        const auto location = glGetUniformLocation(shader.ID, name);
        require(location >= 0, "expected shader uniform is inactive");
        GLint value = 0;
        glGetUniformiv(shader.ID, location, &value);
        return value;
    }

    void verifyCamera(const Shader& shader, const CameraData& camera)
    {
        float position[3];
        glGetUniformfv(shader.ID, glGetUniformLocation(shader.ID, "viewPos"), position);
        for (int i = 0; i < 3; ++i)
            require(std::fabs(position[i] - camera.position[i]) < 0.0001f, "renderer used a stale camera");
    }

    void verifyOutput(RenderOutput output, RenderExtent extent)
    {
        require(output.extent.width == extent.width && output.extent.height == extent.height,
            "output extent mismatch");
        require(glIsTexture(output.colorTexture), "output texture missing");
        glBindTexture(GL_TEXTURE_2D, output.colorTexture);
        GLint width = 0, height = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
        require(width == extent.width && height == extent.height, "output allocation mismatch");
        glBindTexture(GL_TEXTURE_2D, 0);
        require(glGetError() == GL_NO_ERROR, "render submission produced a GL error");
    }

    std::array<float, 4> readPixel(GLuint texture, RenderExtent extent)
    {
        std::vector<float> pixels(static_cast<std::size_t>(extent.width) * extent.height * 4);
        glBindTexture(GL_TEXTURE_2D, texture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
        const auto center = (static_cast<std::size_t>(extent.height / 2) * extent.width + extent.width / 2) * 4;
        return {pixels[center], pixels[center + 1], pixels[center + 2], pixels[center + 3]};
    }

    void postProcessPassTests(ResourceManager& resources)
    {
        using namespace Rendering;
        const RenderExtent extent{32, 24};
        RenderTargets targets;
        targets.initialize(extent, 16, ColorFormat::RGBA16F);
        BloomPass bloom(resources);
        ToneMappingPass toneMapping(resources);
        const auto quad = resources.GetScreenQuad();
        CameraData camera;
        RenderSettings settings;
        settings.postProcess.enabled = true;
        settings.postProcess.bloom = true;
        const RenderFrameData frame{};
        const RenderPassContext context{camera, settings, frame, {}, glm::mat4(1), extent};

        glBindFramebuffer(GL_FRAMEBUFFER, targets.hdr->getFBO());
        const float base[] = {0.25f, 0.5f, 1.0f, 1.0f};
        const float bright[] = {0.5f, 0.25f, 0.125f, 1.0f};
        glClearBufferfv(GL_COLOR, 0, base);
        glClearBufferfv(GL_COLOR, 1, bright);
        // A pass must not inherit the viewport of a previous UI/debug draw.
        glViewport(0, 0, 1, 1);
        const auto blurred = bloom.execute(context, targets.hdr->getColor(1), targets.bloomPingPong, *quad);
        require(blurred == targets.bloomPingPong[1]->getColor(), "five-pass bloom returned the wrong ping-pong target");
        auto pixel = readPixel(blurred, extent);
        for (int i = 0; i < 3; ++i)
            require(std::fabs(pixel[i] - bright[i]) < 0.005f, "bloom failed to preserve a constant image");

        for (bool useBloom : {true, false})
        {
            settings.postProcess.bloom = useBloom;
            glViewport(0, 0, 1, 1);
            toneMapping.execute(context, targets.hdr->getColor(), useBloom ? blurred : 0,
                *targets.finalOutput, *quad);
            pixel = readPixel(targets.finalOutput->getColor(), extent);
            for (int i = 0; i < 3; ++i)
            {
                const float linear = base[i] + (useBloom ? bright[i] : 0.0f);
                const float expected = std::pow(linear / (linear + 1.0f), 1.0f / 2.2f);
                require(std::fabs(pixel[i] - expected) < 0.012f, "tone mapping consumed incorrect scene/bloom input");
            }
        }
        require(glGetError() == GL_NO_ERROR, "standalone post-process passes produced a GL error");
    }

    void deferredDepthTests(ResourceManager& resources, const RenderScene& scene, const CameraData& camera)
    {
        using namespace Rendering;
        const RenderExtent extent{64, 48};
        RenderTargets targets;
        targets.initialize(extent, 16, ColorFormat::RGBA16F);
        GBufferPass geometry(resources);
        DeferredLightingPass lighting(resources);
        const auto plane = resources.GetPlane();
        const auto quad = resources.GetScreenQuad();
        const RenderSettings settings;
        const RenderFrameData frame{};
        const RenderPassContext context{camera, settings, frame, {}, glm::mat4(1), extent};
        geometry.execute(scene, context, *targets.gbuffer, *plane);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, targets.gbuffer->getFBO());
        float geometryDepth = 1.0f;
        glReadPixels(32, 24, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &geometryDepth);
        require(geometryDepth > 0 && geometryDepth < 1, "G-buffer geometry did not write depth");
        lighting.execute(scene, context, {*targets.directionalShadow, targets.pointShadows},
            *targets.gbuffer, *targets.deferredLighting, *quad);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, targets.deferredLighting->getFBO());
        float lightingDepth = 1.0f;
        glReadPixels(32, 24, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &lightingDepth);
        require(std::fabs(geometryDepth - lightingDepth) < 0.00001f,
            "deferred lighting did not copy geometry depth for markers/skybox");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        require(glGetError() == GL_NO_ERROR, "deferred depth handoff produced a GL error");
    }

    void pipelineOrderTests(Renderer& renderer)
    {
        RenderScene scene;
        PointLightData light;
        light.enabled = true;
        light.position = glm::vec3(0);
        light.color = glm::vec3(1, 0, 0);
        scene.pointLights.push_back(light);
        CameraData camera;
        camera.position = glm::vec3(0, 0, 3);
        camera.view = glm::lookAt(camera.position, glm::vec3(0), glm::vec3(0, 1, 0));
        camera.projection = glm::perspective(glm::radians(45.0f), 64.0f / 48.0f, 0.1f, 100.0f);
        RenderSettings settings;
        settings.drawLights = true;
        settings.postProcess.enabled = true;
        for (bool deferred : {false, true})
        {
            settings.deferred = deferred;
            const auto output = renderer.render(scene, camera, settings, {0, false, true});
            const auto pixel = readPixel(output.colorTexture, output.extent);
            require(std::fabs(pixel[0] - std::pow(0.5f, 1.0f / 2.2f)) < 0.012f &&
                pixel[1] < 0.01f && pixel[2] < 0.01f,
                "marker/skybox/post-process ordering or depth state changed");
            verifyOutput(output, {64, 48});
        }
    }

    struct ModelFixture
    {
        std::filesystem::path directory;
        ModelFixture()
        {
            directory = std::filesystem::temp_directory_path() /
                ("hpRenderer-submit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
            require(std::filesystem::create_directory(directory), "create model fixture directory");
            std::ofstream obj(directory / "triangle.obj");
            obj << "v -1 -1 0\nv 1 -1 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n";
            require(obj.good(), "write model fixture");
        }
        ~ModelFixture()
        {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored); // Only this uniquely-created test directory.
        }
    };

    void gpuTests()
    {
        auto& resources = ResourceManager::GetInstance();
        ModelFixture fixture;
        const auto shaderDirectory = fixture.directory / "shaders";
        std::filesystem::copy(std::filesystem::path(HPRENDERER_SOURCE_DIR) / "shaders",
            shaderDirectory, std::filesystem::copy_options::recursive);
        const std::string shaders = shaderDirectory.generic_string() + "/";
        for (const auto& entry : {std::pair<const char*, const char*>{"model", "model"}, {"light", "light"},
            {"scene framebuffer", "framebuffer"}, {"skybox", "skybox"}, {"dir shadow", "shadow"},
            {"bloomBlur", "bloomBlur"}, {"gBuffer", "gBuffer"}, {"lightPass", "lightPass"},
            {"debug", "debug"}, {"gbuffer debug", "drawDebug"}, {"background", "background"},
            {"irradiance", "irradiance"}, {"prefilter", "prefilter"}, {"brdf", "brdf"}})
            resources.LoadShader(entry.first, shaders + entry.second + ".vs", shaders + entry.second + ".fs");
        resources.LoadShader("point shadow", shaders + "pointShadow.vs", shaders + "pointShadow.fs", shaders + "pointShadow.gs");

        Renderer renderer; // No Window, InputManager, mutable Camera or bound Scene.
        renderer.init(resources, {64, 48});
        postProcessPassTests(resources);
        pipelineOrderTests(renderer);
        RenderScene first;
        std::weak_ptr<Model> modelLifetime;
        {
            Scene source;
            auto model = std::make_shared<Model>((fixture.directory / "triangle.obj").generic_string());
            require(!model->meshes.empty(), "model fixture did not load");
            modelLifetime = model;
            source.AddObject(model);
            source.AddPointLight(Light(glm::vec3(1), 1, glm::vec3(1, 2, 3), LightType::Point));
            first = BuildRenderScene(source);
        } // Source scene and its local model reference are already gone.
        require(!modelLifetime.expired(), "snapshot must retain shared geometry");
        Scene secondSource;
        secondSource.GetDirLight().setColor(glm::vec3(0.2f));
        const auto second = BuildRenderScene(secondSource);
        Camera camera;
        const auto firstCamera = BuildCameraData(camera, {64, 48});
        deferredDepthTests(resources, first, firstCamera);
        camera.MoveRight(2.0f);
        const auto secondCamera = BuildCameraData(camera, {64, 48});
        const RenderSettings defaults;
        RenderSettings settings;
        settings.shadows = true;
        settings.drawLights = true;
        settings.postProcess.enabled = true;
        settings.postProcess.bloom = true;
        const RenderFrameData lightsOn{12.5f, true, true};
        auto output = renderer.render(first, firstCamera, settings, lightsOn);
        verifyOutput(output, {64, 48});
        const auto modelShader = resources.GetShader("model");
        verifyCamera(*modelShader, firstCamera);
        require(uniformInt(*modelShader, "parallelLight.enabled") == 1 &&
            uniformInt(*modelShader, "pointLight[0].enabled") == 1, "frame light switches not applied");
        const GLuint postOutput = output.colorTexture;

        // Render a different scene/camera/settings immediately using the same renderer.
        output = renderer.render(second, secondCamera, defaults, {17.0f, false, false});
        verifyOutput(output, {64, 48});
        verifyCamera(*modelShader, secondCamera);
        require(output.colorTexture != postOutput && uniformInt(*modelShader, "pointLightCount") == 0 &&
            uniformInt(*modelShader, "parallelLight.enabled") == 0, "scene/settings/light state leaked across submissions");
        require(first.objects.size() == 1 && first.pointLights.size() == 1 && second.objects.empty(),
            "render mutated scene snapshots");

        RenderSettings deferred;
        deferred.deferred = true;
        deferred.drawGBufferDebug = true;
        output = renderer.render(first, firstCamera, deferred, lightsOn);
        verifyOutput(output, {64, 48});
        require(!deferred.postProcess.enabled, "renderer mutated caller's post-process settings");
        verifyCamera(*resources.GetShader("lightPass"), firstCamera);
        require(uniformInt(*resources.GetShader("lightPass"), "pointLight[0].enabled") == 1,
            "deferred path ignored frame light switches");

        // reload() intentionally returns false for unchanged files. Touch only
        // private fixture copies to exercise real program replacement.
        for (const auto& entry : std::filesystem::directory_iterator(shaderDirectory))
            if (entry.is_regular_file())
                std::filesystem::last_write_time(entry.path(),
                    std::filesystem::last_write_time(entry.path()) + std::chrono::seconds(2));
        for (const auto& result : resources.ReloadAllShaders())
            require(result.second, "shader reload failed");
        renderer.restoreShaderBindings();
        require(uniformInt(*modelShader, "irradianceMap") == 11 &&
            uniformInt(*modelShader, "normal") == 7 &&
            uniformInt(*resources.GetShader("lightPass"), "gDepth") == 5 &&
            uniformInt(*resources.GetShader("lightPass"), "prefilterMap") == 12,
            "pass-owned bindings were not restored after shader reload");
        for (bool useDeferred : {false, true})
            for (bool usePost : {false, true})
                for (bool useBloom : {false, true})
                {
                    RenderSettings combination = settings;
                    combination.deferred = useDeferred;
                    combination.postProcess.enabled = usePost;
                    combination.postProcess.bloom = useBloom;
                    combination.drawGBufferDebug = useDeferred;
                    glDisable(GL_DEPTH_TEST);
                    glViewport(0, 0, 1, 1);
                    verifyOutput(renderer.render(first, firstCamera, combination, lightsOn), {64, 48});
                }

        const auto extent = renderer.resize({80, 40});
        output = renderer.render(second, BuildCameraData(camera, extent), deferred, {});
        verifyOutput(output, {80, 40});
        require(renderer.resize({0, 0}).width == 80, "zero viewport request changed active size");
        require(renderer.resize({0xffffffffu, 40}).width == 80, "failed resize did not preserve active size");
        output = renderer.render(second, BuildCameraData(camera, extent), defaults, {});
        verifyOutput(output, {80, 40});
        first = {};
        require(modelLifetime.expired(), "renderer retained submitted scene/model references");
        const auto texture = output.colorTexture;
        renderer.shutdown();
        renderer.shutdown();
        require(!glIsTexture(texture), "renderer shutdown leaked output");
        renderer.init(resources, {32, 32});
        verifyOutput(renderer.render(second, BuildCameraData(camera, {32, 32}), defaults, {}), {32, 32});
        renderer.shutdown();
        resources.Clear();
    }
}

int main()
{
    GLFWwindow* window = nullptr;
    int result = 0;
    try
    {
        require(glfwInit() == GLFW_TRUE, "GLFW init");
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        window = glfwCreateWindow(96, 96, "Renderer submission tests", nullptr, nullptr);
        require(window != nullptr, "hidden GL context");
        glfwMakeContextCurrent(window);
        require(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0, "GL loader");
        gpuTests();
        std::cout << "Renderer GPU submission tests passed.\n";
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
    if (window)
    {
        ResourceManager::GetInstance().Clear();
        glfwDestroyWindow(window);
    }
    glfwTerminate();
    return result;
}
