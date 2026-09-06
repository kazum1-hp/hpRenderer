#include "Editor/EditorLayer.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <stdexcept>

EditorLayer::~EditorLayer()
{
    shutdown();
}

void EditorLayer::initialize(GLFWwindow* window)
{
    if (initialized) return;
    if (!window) throw std::invalid_argument("EditorLayer requires a valid window");
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable |
        ImGuiConfigFlags_ViewportsEnable;
    ImGui::StyleColorsDark();
    auto& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f;
    style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    if (!ImGui_ImplGlfw_InitForOpenGL(window, true))
    {
        ImGui::DestroyContext();
        throw std::runtime_error("Editor GLFW backend initialization failed");
    }
    if (!ImGui_ImplOpenGL3_Init("#version 330"))
    {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("Editor OpenGL backend initialization failed");
    }
    initialized = true;
    dockInitialized = false;
    dialogDockId = 0;
    inputCapture = {};
    viewport.refreshDebugLabels();
}

void EditorLayer::shutdown()
{
    if (!initialized) return;
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    initialized = false;
    inputCapture = {};
}

void EditorLayer::beginFrame()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void EditorLayer::draw(Scene& scene, RenderSettings& settings, const RenderOutput& output,
    InputManager& input, const std::function<void()>& restoreShaderBindings)
{
    drawDockSpace();
    lighting.draw(scene);
    renderSettings.drawPostProcessing(settings, output.extent);
    ImGui::Begin("Renderer Settings");
    inspector.draw(scene);
    renderSettings.draw(settings, input);
    ImGui::End();
    if (assets.draw(scene, dialogDockId))
    {
        if (restoreShaderBindings) restoreShaderBindings();
        viewport.refreshDebugLabels();
    }
    console.draw();
    viewport.draw(output, settings);
    const auto& io = ImGui::GetIO();
    // Scene is itself an ImGui window: navigation may request keyboard capture
    // even though the user intends to control the camera. Keep active controls
    // and text entry protected, but otherwise let the hovered viewport use keys.
    const bool keyboardCaptured = io.WantTextInput || ImGui::IsAnyItemActive() ||
        (io.WantCaptureKeyboard && !viewport.isHovered());
    inputCapture = {io.WantCaptureMouse && !viewport.isHovered(),
        keyboardCaptured, viewport.isHovered()};
}

void EditorLayer::endFrame()
{
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        GLFWwindow* backup = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup);
    }
}

void EditorLayer::drawDockSpace()
{
    // --- DockSpace / Layout setup (full-screen) ---
    ImGuiIO& io = ImGui::GetIO();

    ImGuiID& left_bottom_id = dialogDockId;
    ImGuiID bottom_left_id = 0;

    if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGuiWindowFlags host_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar;

        ImGui::Begin("MainDockSpace", nullptr, host_flags);
        ImGui::PopStyleVar(2);

        ImGuiID dockspace_id = ImGui::GetID("MainDockSpaceID");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);

        // One-time DockBuilder layout
        if (!dockInitialized)
        {
            dockInitialized = true;
            ImGui::DockBuilderRemoveNode(dockspace_id);
            ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

            ImGuiID dock_main_id = dockspace_id;
            ImGuiID left_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.20f, nullptr, &dock_main_id);
            ImGuiID left_top_id = ImGui::DockBuilderSplitNode(left_id, ImGuiDir_Up, 0.5f, nullptr, &left_bottom_id);
            ImGuiID bottom_id = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.20f, nullptr, &dock_main_id);
            ImGuiID bottom_right_id = ImGui::DockBuilderSplitNode(bottom_id, ImGuiDir_Right, 0.65f, nullptr, &bottom_left_id);

            // Dock windows by exact names used below
            ImGui::DockBuilderDockWindow("Light Control", left_top_id);
            ImGui::DockBuilderDockWindow("Renderer Settings", left_bottom_id);
            ImGui::DockBuilderDockWindow("Post Processing", left_bottom_id);
            ImGui::DockBuilderDockWindow("Scene", dock_main_id);
            ImGui::DockBuilderDockWindow("Reload Shaders", bottom_left_id);
            ImGui::DockBuilderDockWindow("Reload Assets", bottom_left_id);
            ImGui::DockBuilderDockWindow("Console", bottom_right_id);

            ImGui::DockBuilderFinish(dockspace_id);
        }

        ImGui::End(); // End MainDockSpace
    }

}
