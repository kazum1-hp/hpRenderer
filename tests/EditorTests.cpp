#include "Editor/EditorLayer.h"
#include "Scene.h"
#include "InputManager.h"
#include "FrameBuffer.h"
#include "RenderSettings.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <iostream>
#include <stdexcept>

namespace
{
    void require(bool value, const char* message)
    {
        if (!value) throw std::runtime_error(message);
    }

    void drawFrames(EditorLayer& editor, bool gpu, unsigned int texture = 0)
    {
        Camera camera;
        InputManager input(camera);
        Scene scene;
        RenderSettings settings;
        RenderOutput output{texture, {320, 240}};
        scene.AddObject(nullptr);
        scene.AddObject(nullptr);
        scene.GetObjects()[1].material.roughnessBias = 0.75f;
        scene.AddPointLight(Light(glm::vec3(1), 2.0f, glm::vec3(0), LightType::Point));

        for (int frame = 0; frame < 6; ++frame)
        {
            if (frame == 2) scene.RemoveObject(0);
            if (frame == 3) scene.RemoveObject(0);
            if (frame == 4) scene.AddObject(nullptr);
            settings.deferred = frame % 2 != 0;
            settings.drawGBufferDebug = true;
            settings.postProcess.enabled = true;
            settings.drawLights = true;
            if (gpu) editor.beginFrame(); else ImGui::NewFrame();
            editor.draw(scene, settings, output, input, [] {});
            if (gpu) editor.endFrame(); else ImGui::Render();
            require(ImGui::GetDrawData() != nullptr, "missing editor draw data");
            if (frame == 2)
                require(scene.GetObjects()[0].material.roughnessBias == 0.75f, "panel draw changed material");
            require(output.colorTexture == texture, "editor mutated renderer output");
            if (gpu) require(glGetError() == GL_NO_ERROR, "editor backend GL error");
        }
        require(editor.requestedExtent().isValid(), "viewport did not request a valid extent");
        for (const char* title : {"Light Control", "Renderer Settings", "Post Processing",
            "Scene", "Reload Shaders", "Reload Assets", "Console"})
            require(ImGui::FindWindowByName(title) != nullptr, "missing preserved editor window");
    }

    void panelTests()
    {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr; // Tests must not overwrite the user's docking layout.
        io.DisplaySize = ImVec2(1280, 800);
        io.DeltaTime = 1.0f / 60.0f;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        {
            EditorLayer editor;
            drawFrames(editor, false, 42);

            Camera camera;
            InputManager input(camera);
            Scene scene;
            RenderSettings settings;
            RenderOutput output{42, {320, 240}};
            auto* sceneWindow = ImGui::FindWindowByName("Scene");
            ImGui::FocusWindow(sceneWindow);
            // Reproduce navigation capture while the mouse is over Scene.
            for (int frame = 0; frame < 3; ++frame)
            {
                io.AddMousePosEvent(sceneWindow->Pos.x + sceneWindow->Size.x * 0.5f,
                    sceneWindow->Pos.y + sceneWindow->Size.y * 0.5f);
                ImGui::NewFrame();
                io.WantCaptureKeyboard = true;
                io.WantTextInput = frame == 2;
                editor.draw(scene, settings, output, input, [] {});
                require(editor.captureState().viewportHovered, "capture test must hover Scene");
                require(editor.captureState().keyboardCaptured == (frame == 2),
                    "Scene must allow navigation keys, but not keys during text entry");
                ImGui::Render();
            }
            io.AddMousePosEvent(-100, -100);
            ImGui::NewFrame();
            io.WantCaptureKeyboard = true;
            io.WantTextInput = false;
            editor.draw(scene, settings, output, input, [] {});
            require(!editor.captureState().viewportHovered && editor.captureState().keyboardCaptured,
                "UI keyboard capture outside Scene must remain protected");
            ImGui::Render();
        }
        ImGui::DestroyContext();
    }

    void controlTests()
    {
        ImGui::CreateContext();
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(1280, 800);
        io.DeltaTime = 1.0f / 60.0f;
        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        Transform transform;
        transform.setInitialTransform(glm::vec3(2), glm::vec3(3));
        transform.setPosition(glm::vec3(7));
        transform.setRotation(glm::vec3(1));
        Light light(glm::vec3(1), 1, glm::vec3(0), LightType::Point);
        Camera camera;
        InputManager input(camera);
        RenderSettings settings;
        Scene scene;
        scene.AddObject(nullptr);
        scene.AddObject(nullptr);
        scene.GetObjects()[1].material.aoBias = 0.7f;
        {
            AssetPanel assets;
            RenderSettingsPanel settingsPanel;
            for (int frame = 0; frame < 8; ++frame)
            {
                ImGui::NewFrame();
                ImGui::SetNextWindowPos(ImVec2(0, 0));
                ImGui::SetNextWindowSize(ImVec2(400, 700));
                ImGui::Begin("Controls");
                // Queue activation for the following frame, using real widget IDs.
                if (frame == 1) ImGui::ActivateItemByID(ImGui::GetID("Reset"));
                if (frame == 3) ImGui::ActivateItemByID(ImGui::GetID("useShadow"));
                if (frame == 5)
                {
                    ImGui::PushID(0);
                    ImGui::ActivateItemByID(ImGui::GetID("Enabled"));
                    ImGui::PopID();
                }
                InspectorPanel::drawTransform(transform);
                settingsPanel.draw(settings, input);
                LightingPanel::drawPointLight(light, 0);
                ImGui::End();
                // Draw asset controls without docking so the delete button is visible.
                ImGui::SetNextWindowPos(ImVec2(450, 0));
                ImGui::SetNextWindowSize(ImVec2(500, 300));
                ImGui::Begin("Reload Shaders");
                ImGui::End();
                ImGui::SetNextWindowPos(ImVec2(450, 310));
                ImGui::SetNextWindowSize(ImVec2(700, 480));
                ImGui::Begin("Reload Assets");
                ImGui::End();
                assets.draw(scene, 0);
                if (frame == 6)
                    ImGui::ActivateItemByID(ImGui::FindWindowByName("Reload Assets")->GetID("Delete Selected Model"));
                ImGui::Render();
            }
            require(transform.getPosition() == glm::vec3(2) && transform.getRotation() == glm::vec3(0)
                && transform.getScale() == glm::vec3(3), "Inspector Reset button");
            require(settings.shadows, "render settings checkbox");
            require(!light.lightOn(), "light enabled checkbox");
            require(scene.GetObjects().size() == 1 && scene.GetObjects()[0].material.aoBias == 0.7f,
                "asset delete button / surviving object state");
        }
        ImGui::DestroyContext();
    }

    void gpuTests(GLFWwindow* window)
    {
        FramebufferDesc desc;
        desc.extent = {320, 240};
        desc.colors = {{ColorFormat::RGB8}};
        desc.debugName = "Editor smoke image";
        FrameBuffer framebuffer(desc);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer.getFBO());
        glClearColor(0.2f, 0.4f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        for (int cycle = 0; cycle < 2; ++cycle)
        {
            EditorLayer editor;
            editor.initialize(window);
            ImGui::GetIO().IniFilename = nullptr;
            drawFrames(editor, true, framebuffer.getColor());
            editor.shutdown();
            editor.shutdown(); // Idempotent, including destruction after explicit shutdown.
            require(ImGui::GetCurrentContext() == nullptr, "editor context leaked");
            require(glfwGetCurrentContext() == window, "platform viewport did not restore GL context");
        }
        require(glIsTexture(framebuffer.getColor()) == GL_TRUE, "editor took output texture ownership");
    }
}

int main(int argc, char** argv)
{
    const bool gpu = argc > 1 && std::string(argv[1]) == "--gpu";
    GLFWwindow* window = nullptr;
    int result = 0;
    try
    {
        if (gpu)
        {
            require(glfwInit() == GLFW_TRUE, "GLFW initialization");
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            window = glfwCreateWindow(1280, 800, "Editor smoke tests", nullptr, nullptr);
            require(window != nullptr, "hidden editor context");
            glfwMakeContextCurrent(window);
            require(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0, "GL loader");
            gpuTests(window);
        }
        else { panelTests(); controlTests(); }
        std::cout << (gpu ? "Editor GPU smoke passed.\n" : "Editor panel tests passed.\n");
    }
    catch (const std::exception& error)
    {
        if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
        std::cerr << error.what() << '\n';
        result = 1;
    }
    if (window) glfwDestroyWindow(window);
    if (gpu) glfwTerminate();
    return result;
}
