#include "hpr/renderer/Renderer.h"
#include "hpr/renderer/ModelGpuCache.h"
#include "hpr/renderer/RenderExtraction.h"
#include "hpr/assets/AssetManager.h"
#include "hpr/renderer/opengl/Shader.h"
#include "hpr/renderer/opengl/Texture.h"
#include <GLFW/glfw3.h>
#include "hpr/scene/Scene.h"
#include "hpr/scene/Camera.h"
#include "hpr/renderer/passes/RenderPasses.h"
#include "hpr/renderer/RenderTargets.h"
#include "hpr/renderer/opengl/PrimitiveMeshes.h"
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    std::string skyboxFixtureDirectory;
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

    void postProcessPassTests(AssetManager& resources)
    {
        using namespace Rendering;
        const RenderExtent extent{32, 24};
        RenderTargets targets;
        targets.initialize(extent, 16, ColorFormat::RGBA16F);
        BloomPass bloom(resources);
        ToneMappingPass toneMapping(resources);
        const auto quad = Rendering::CreateScreenQuad();
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

    void deferredDepthTests(AssetManager& resources, const RenderScene& source, const CameraData& camera)
    {
        ModelGpuCache cache(resources);
        const auto scene = cache.prepare(source);
        using namespace Rendering;
        const RenderExtent extent{64, 48};
        RenderTargets targets;
        targets.initialize(extent, 16, ColorFormat::RGBA16F);
        GBufferPass geometry(resources);
        DeferredLightingPass lighting(resources);
        const auto plane = Rendering::CreatePlane();
        const auto quad = Rendering::CreateScreenQuad();
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
        for (bool post : {false, true})
        {
            settings.deferred = deferred;
            settings.postProcess = {};
            settings.postProcess.enabled = post;
            if (!post)
            {
                // Hidden editor values must not affect the default display conversion.
                settings.postProcess.bloom = true;
                settings.postProcess.hdr = false;
                settings.postProcess.exposure = 8.0f;
                settings.postProcess.effectMode = 1;
                settings.postProcess.toneMappingMode = 2;
                settings.postProcess.scanPosition = 64;
            }
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

    PFNGLCREATESHADERPROC originalCreateShader;
    PFNGLCREATEPROGRAMPROC originalCreateProgram;
    std::vector<GLuint> createdShaders, createdPrograms;
    GLuint APIENTRY trackShader(GLenum type)
    {
        const auto id = originalCreateShader(type);
        createdShaders.push_back(id);
        return id;
    }
    GLuint APIENTRY trackProgram()
    {
        const auto id = originalCreateProgram();
        createdPrograms.push_back(id);
        return id;
    }

    void shaderFailureTests(const std::filesystem::path& directory)
    {
        const auto vs = directory / "failure.vs", fs = directory / "failure.fs", gs = directory / "failure.gs";
        const std::string vertex = "#version 330 core\nvoid main(){gl_Position=vec4(0,0,0,1);}";
        const std::string fragment = "#version 330 core\nout vec4 color; void main(){color=vec4(1);}";
        const std::string geometry = "#version 330 core\nlayout(points) in; layout(points,max_vertices=1) out;"
            "void main(){gl_Position=gl_in[0].gl_Position; EmitVertex(); EndPrimitive();}";
        auto timestamp = std::filesystem::file_time_type::clock::now();
        const auto write = [&](const auto& path, const std::string& code) {
            { std::ofstream file(path); file << code; require(file.good(), "write shader fixture"); }
            timestamp += std::chrono::seconds(2);
            std::filesystem::last_write_time(path, timestamp);
        };
        write(vs, vertex); write(fs, fragment); write(gs, geometry);
        Shader shader(vs.generic_string(), fs.generic_string(), gs.generic_string());
        const auto old = shader.ID;
        // Repeat every failure, observing actual GL allocations rather than assuming IDs are consecutive.
        for (int iteration = 0; iteration < 2; ++iteration)
            for (int stage = 0; stage < 4; ++stage)
            {
                write(vs, stage == 0 ? "invalid vertex" : vertex);
                write(fs, stage == 1 ? "invalid fragment" : stage == 3
                    ? "#version 330 core\nin vec3 missing; out vec4 color; void main(){color=vec4(missing,1);}"
                    : fragment);
                write(gs, stage == 2 ? "invalid geometry" : geometry);
                createdShaders.clear(); createdPrograms.clear();
                originalCreateShader = glad_glCreateShader;
                originalCreateProgram = glad_glCreateProgram;
                glad_glCreateShader = trackShader;
                glad_glCreateProgram = trackProgram;
                const bool reloaded = shader.reload();
                glad_glCreateShader = originalCreateShader;
                glad_glCreateProgram = originalCreateProgram;
                require(!reloaded && shader.ID == old, "failed reload replaced the working program");
                for (auto id : createdShaders) require(!glIsShader(id), "failed reload leaked a shader object");
                for (auto id : createdPrograms) require(!glIsProgram(id), "failed reload leaked a program");
                shader.use();
                require(glGetError() == GL_NO_ERROR, "old shader unusable after reload failure");
            }
        write(vs, vertex); write(fs, fragment); write(gs, geometry);
        require(shader.reload() && shader.ID != old, "shader did not recover after compile/link failures");
        shader.use();
        require(!glIsProgram(old), "successful reload retained the old program");
        glUseProgram(0);
    }

    void gpuTests()
    {
        AssetManager resources;
        ModelFixture fixture;
        shaderFailureTests(fixture.directory);
        const auto shaderDirectory = fixture.directory / "shaders";
        std::filesystem::copy(std::filesystem::path(HPRENDERER_SOURCE_DIR) / "shaders",
            shaderDirectory, std::filesystem::copy_options::recursive);
        const std::string shaders = shaderDirectory.generic_string() + "/";
        for (const auto& entry : {std::pair<ShaderId, const char*>{ShaderId::Model, "model"}, {ShaderId::Light, "light"},
            {ShaderId::ToneMapping, "framebuffer"}, {ShaderId::EnvironmentCapture, "skybox"}, {ShaderId::DirectionalShadow, "shadow"},
            {ShaderId::BloomBlur, "bloomBlur"}, {ShaderId::GBuffer, "gBuffer"}, {ShaderId::DeferredLighting, "lightPass"},
            {ShaderId::Debug, "debug"}, {ShaderId::GBufferDebug, "drawDebug"}, {ShaderId::Skybox, "background"},
            {ShaderId::Irradiance, "irradiance"}, {ShaderId::Prefilter, "prefilter"}, {ShaderId::Brdf, "brdf"}})
            resources.LoadShader(entry.first, shaders + entry.second + ".vs", shaders + entry.second + ".fs");
        resources.LoadShader(ShaderId::PointShadow, shaders + "pointShadow.vs", shaders + "pointShadow.fs", shaders + "pointShadow.gs");

        Renderer renderer; // No Window, InputManager, mutable Camera or bound Scene.
        renderer.init(resources, {64, 48});
        postProcessPassTests(resources);
        pipelineOrderTests(renderer);
        {
            auto generated = std::make_shared<SkyboxAsset>();
            for (int f = 0; f < 6; ++f)
            {
                auto& face = generated->faces[f];
                face.width = face.height = 8;
                for (int y = 0; y < 8; ++y)
                    for (int x = 0; x < 8; ++x)
                        face.rgba.insert(face.rgba.end(), {static_cast<unsigned char>(30 + x * 25),
                            static_cast<unsigned char>(30 + y * 25), static_cast<unsigned char>(30 + f * 30), 255});
            }
            std::shared_ptr<const SkyboxAsset> source = generated;
            if (!skyboxFixtureDirectory.empty())
            {
                std::array<std::string, 6> paths;
                const char* names[] = {"px.png", "nx.png", "py.png", "ny.png", "pz.png", "nz.png"};
                for (int f = 0; f < 6; ++f)
                    paths[f] = (std::filesystem::u8path(skyboxFixtureDirectory) / names[f]).generic_u8string();
                std::string error;
                source = SkyboxAsset::Load(paths, error);
                require(source != nullptr, "external skybox diagnostic fixture failed");
            }
            RenderScene scene;
            scene.environmentMode = EnvironmentMode::SixFaces;
            scene.skybox = source;
            const glm::vec3 directions[] = {{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};
            CameraData camera;
            camera.projection = glm::perspective(glm::radians(60.0f), 64.0f/48.0f, .1f, 100.f);
            auto linear = [](float s) { return s <= .04045f ? s / 12.92f : std::pow((s+.055f)/1.055f,2.4f); };
            for (bool deferred : {false, true})
                for (int f = 0; f < 6; ++f)
                {
                    camera.view = glm::lookAt(glm::vec3(0), directions[f],
                        f == 2 ? glm::vec3(0,0,1) : f == 3 ? glm::vec3(0,0,-1) : glm::vec3(0,1,0));
                    RenderSettings settings;
                    settings.deferred = deferred;
                    const auto output = renderer.render(scene, camera, settings, {});
                    std::vector<float> pixels(64*48*4);
                    glBindTexture(GL_TEXTURE_2D, output.colorTexture);
                    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
                    for (int y : {8, 24, 39}) for (int x : {8, 32, 55})
                    {
                        const auto ray = glm::vec3(glm::inverse(camera.view) * glm::inverse(camera.projection) *
                            glm::vec4(2.f*(x+.5f)/64-1, 2.f*(y+.5f)/48-1, 1, 1));
                        float s=0,t=0,m=0;
                        switch(f) {
                        case 0: s=-ray.z; t=-ray.y; m=ray.x; break;
                        case 1: s=ray.z; t=-ray.y; m=-ray.x; break;
                        case 2: s=ray.x; t=ray.z; m=ray.y; break;
                        case 3: s=ray.x; t=-ray.z; m=-ray.y; break;
                        case 4: s=ray.x; t=-ray.y; m=ray.z; break;
                        case 5: s=-ray.x; t=-ray.y; m=-ray.z; break;
                        }
                        const auto& face=source->faces[f];
                        float u=(s/m+1)*.5f*face.width-.5f, v=(t/m+1)*.5f*face.height-.5f;
                        int ix=static_cast<int>(std::floor(u)), iy=static_cast<int>(std::floor(v));
                        float fx=u-ix, fy=v-iy;
                        for (int c=0;c<3;++c) {
                            auto sample=[&](int dx,int dy) {
                                return linear(face.rgba[((iy+dy)*face.width+ix+dx)*4+c]/255.f);
                            };
                            float expected=glm::mix(glm::mix(sample(0,0),sample(1,0),fx),
                                glm::mix(sample(0,1),sample(1,1),fx),fy);
                            expected=std::pow(expected/(expected+1),1.f/2.2f);
                            if (std::abs(pixels[(y*64+x)*4+c]-expected) > .035f)
                                throw std::runtime_error("Skybox orientation mismatch face="+std::to_string(f)+
                                    " x="+std::to_string(x)+" y="+std::to_string(y));
                        }
                    }
                }
            std::cout << "Skybox six-face orientation pixel checks passed.\n";
        }
        {
            const auto hdrPath = fixture.directory / "environment.hdr";
            {
                std::ofstream file(hdrPath, std::ios::binary);
                file << "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 2\n";
                const unsigned char rgbe[] = {128, 64, 32, 129};
                for (int i = 0; i < 4; ++i) file.write(reinterpret_cast<const char*>(rgbe), sizeof(rgbe));
            }
            RenderScene environmentScene;
            environmentScene.environment = resources.LoadEnvironment(hdrPath.generic_u8string());
            require(environmentScene.environment != nullptr, "HDR switch fixture failed");
            auto faces = std::make_shared<SkyboxAsset>();
            for (auto& face : faces->faces)
            {
                face.width = face.height = 1;
                face.rgba = {0, 255, 0, 255};
            }
            environmentScene.skybox = faces;
            CameraData environmentCamera;
            environmentCamera.view = glm::mat4(1);
            environmentCamera.projection = glm::perspective(glm::radians(45.0f), 64.0f / 48.0f, .1f, 100.0f);
            for (bool useDeferred : {false, true})
            {
                RenderSettings environmentSettings;
                environmentSettings.deferred = useDeferred;
                const auto lightingShader = resources.GetShader(useDeferred ? ShaderId::DeferredLighting : ShaderId::Model);
                for (auto mode : {EnvironmentMode::IBL, EnvironmentMode::SixFaces, EnvironmentMode::Disabled,
                                  EnvironmentMode::SixFaces, EnvironmentMode::IBL})
                {
                    environmentScene.environmentMode = mode;
                    glClearColor(0, 0, 0, 1);
                    const auto output = renderer.render(environmentScene, environmentCamera, environmentSettings, {});
                    verifyOutput(output, {64, 48});
                    require(uniformInt(*lightingShader, "useIBL") == (mode == EnvironmentMode::IBL),
                            "environment mode leaked stale IBL lighting");
                    const auto pixel = readPixel(output.colorTexture, output.extent);
                    if (mode == EnvironmentMode::SixFaces)
                        require(pixel[1] > .5f && pixel[0] < .01f && pixel[2] < .01f, "six-face skybox not drawn");
                    else if (mode == EnvironmentMode::Disabled)
                        require(pixel[0] < .01f && pixel[1] < .01f && pixel[2] < .01f, "disabled environment retained background");
                    else
                        require(pixel[0] > pixel[1] && pixel[1] > pixel[2] && pixel[2] > .1f, "HDR did not survive mode switch");
                }
            }
        }
        RenderScene first;
        std::weak_ptr<Model> modelLifetime;
        {
            Scene source;
            auto model = std::make_shared<Model>((fixture.directory / "triangle.obj").generic_string());
            require(model->isValid(), "model fixture did not load");
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
        {
            auto statisticsScene = first;
            statisticsScene.environmentMode = EnvironmentMode::Disabled;
            RenderSettings measured;
            verifyOutput(renderer.render(statisticsScene, firstCamera, measured, {}), {64, 48});
            require(renderer.statistics().drawCalls == 2 && renderer.statistics().triangles == 3 &&
                    renderer.statistics().renderedObjects == 1, "forward submission statistics incorrect");
            measured.postProcess.enabled = measured.postProcess.bloom = true;
            verifyOutput(renderer.render(statisticsScene, firstCamera, measured, {}), {64, 48});
            require(renderer.statistics().drawCalls == 7 && renderer.statistics().triangles == 13 &&
                    renderer.statistics().renderedObjects == 1, "bloom cost not reflected in statistics");
            measured.postProcess.bloom = false;
            measured.deferred = measured.drawGBufferDebug = true;
            verifyOutput(renderer.render(statisticsScene, firstCamera, measured, {}), {64, 48});
            require(renderer.statistics().drawCalls == 7 && renderer.statistics().triangles == 13,
                    "deferred/debug submissions missing from statistics");
            const auto sampledFrame = renderer.statistics().frameNumber;
            glFinish(); // Test-only synchronization: production profiler must never do this.
            verifyOutput(renderer.render(statisticsScene, firstCamera, measured, {}), {64, 48});
            const auto& stats = renderer.statistics();
            require(stats.gpuValid && stats.gpuFrameNumber == sampledFrame, "real GPU query did not resolve");
            require(stats.gpuQueries.front().drawCalls == 7 && stats.gpuQueries.front().triangles == 13 &&
                    std::isfinite(stats.gpuQueries.front().milliseconds), "GPU sample counters/timing incorrect");
            measured.groundPlane.visible = true;
            verifyOutput(renderer.render(statisticsScene, firstCamera, measured, {}), {64, 48});
            require(renderer.statistics().renderedObjects == 2, "ground plane not counted");
            statisticsScene.objects[0].transform = glm::mat4(0);
            verifyOutput(renderer.render(statisticsScene, firstCamera, measured, {}), {64, 48});
            require(renderer.statistics().renderedObjects == 1, "skipped singular object counted as rendered");
        }
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
        const auto modelShader = resources.GetShader(ShaderId::Model);
        verifyCamera(*modelShader, firstCamera);
        require(uniformInt(*modelShader, "parallelLight.enabled") == 1 &&
            uniformInt(*modelShader, "pointLight[0].enabled") == 1, "frame light switches not applied");
        const GLuint postOutput = output.colorTexture;

        // Render a different scene/camera/settings immediately using the same renderer.
        output = renderer.render(second, secondCamera, defaults, {17.0f, false, false});
        verifyOutput(output, {64, 48});
        verifyCamera(*modelShader, secondCamera);
        require(output.colorTexture == postOutput, "all modes must return the final display target");
        require(uniformInt(*resources.GetShader(ShaderId::ToneMapping), "useBloom") == 0 &&
            uniformInt(*resources.GetShader(ShaderId::ToneMapping), "effectMode") == 0,
            "disabled post-processing retained optional effects");
        require(uniformInt(*modelShader, "pointLightCount") == 0 &&
            uniformInt(*modelShader, "parallelLight.enabled") == 0, "scene/settings/light state leaked across submissions");
        require(first.objects.size() == 1 && first.pointLights.size() == 1 && second.objects.empty(),
            "render mutated scene snapshots");

        RenderSettings deferred;
        deferred.deferred = true;
        deferred.drawGBufferDebug = true;
        output = renderer.render(first, firstCamera, deferred, lightsOn);
        verifyOutput(output, {64, 48});
        require(!deferred.postProcess.enabled, "renderer mutated caller's post-process settings");
        verifyCamera(*resources.GetShader(ShaderId::DeferredLighting), firstCamera);
        require(uniformInt(*resources.GetShader(ShaderId::DeferredLighting), "pointLight[0].enabled") == 1,
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
            uniformInt(*resources.GetShader(ShaderId::DeferredLighting), "gDepth") == 5 &&
            uniformInt(*resources.GetShader(ShaderId::DeferredLighting), "prefilterMap") == 12,
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

int main(int argc, char** argv)
{
    if (argc > 1) skyboxFixtureDirectory = argv[1];
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
        glfwDestroyWindow(window);
    }
    glfwTerminate();
    return result;
}
