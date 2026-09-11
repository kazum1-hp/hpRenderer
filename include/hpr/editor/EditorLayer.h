#pragma once
#include "hpr/editor/panels/Panels.h"
#include "hpr/core/InputCaptureState.h"
#include "hpr/renderer/RendererStatistics.h"
#include <functional>

struct GLFWwindow;

// Owns all editor UI state and its backend lifetime. The renderer has no dependency on this layer.
class EditorLayer
{
public:
    explicit EditorLayer(AssetManager& resources) : resources(resources), assets(resources) {};
    ~EditorLayer();
    EditorLayer(const EditorLayer&) = delete;
    EditorLayer& operator=(const EditorLayer&) = delete;
    void initialize(GLFWwindow* window);
    void shutdown();
    void beginFrame();
    void draw(Scene& scene, RenderSettings& settings, const RenderOutput& output,
        InputManager& input, const std::function<void()>& restoreShaderBindings);
    void endFrame();
    void drawStatistics(const Rendering::RendererStatistics& stats, double frameMs, double cpuFrameMs);
    RenderExtent requestedExtent() const { return viewport.requestedExtent(); }
    InputCaptureState captureState() const { return inputCapture; }
private:
    AssetManager& resources;
    void drawDockSpace();
    bool initialized = false;
    bool dockInitialized = false;
    unsigned int dialogDockId = 0;
    InputCaptureState inputCapture;
    InspectorPanel inspector;
    LightingPanel lighting;
    RenderSettingsPanel renderSettings;
    AssetPanel assets;
    ConsolePanel console;
    SceneViewportPanel viewport;
};
