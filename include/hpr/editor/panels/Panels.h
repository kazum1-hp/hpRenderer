#pragma once
#include "hpr/editor/ConsoleCapture.h"
#include "hpr/renderer/RenderTypes.h"
#include <array>
#include <memory>
#include <string>

class AssetManager;
class Scene;
class Transform;
class Light;
class InputManager;
struct RenderSettings;
namespace IGFD { class FileDialog; }

class InspectorPanel
{
public:
    // Draws contents inside Renderer Settings, preserving the existing layout.
    void draw(Scene& scene);
    static void drawTransform(Transform& transform);
private:
    int selectedIndex = 0;
};

class LightingPanel
{
public:
    void draw(Scene& scene);
    static void drawDirectionalLight(Light& light);
    static void drawPointLight(Light& light, int index);
};

class RenderSettingsPanel
{
public:
    void draw(RenderSettings& settings, InputManager& input);
    void drawPostProcessing(RenderSettings& settings, RenderExtent extent);
};

class AssetPanel
{
public:
    explicit AssetPanel(AssetManager& resources);
    ~AssetPanel();
    // Returns true when at least one shader was successfully reloaded.
    bool draw(Scene& scene, unsigned int dialogDockId);
private:
    AssetManager& resources;
    std::unique_ptr<IGFD::FileDialog> fileDialog;
    char modelPathBuf[1024] = "../assets/models/blue_metal_plate_4k.gltf/blue_metal_plate_4k.gltf";
    char hdrPathBuf[1024] = "../assets/hdr/newport_loft.hdr";
    std::string lastReloadMsg = "Idle";
    std::string hotReloadMsg = "Idle";
    int assetSelectedIndex = 0;
    int pendingModelAction = 0; // none, replace selected, add
    std::array<std::array<char, 1024>, 6> skyboxPaths{};
    int pendingSkyboxFace = -1;
};

class ConsolePanel
{
public:
    void draw();
private:
    ConsoleCapture capture;
};

class SceneViewportPanel
{
public:
    void draw(const RenderOutput& output, const RenderSettings& settings);
    void refreshDebugLabels(const AssetManager& resources);
    RenderExtent requestedExtent() const { return requestedSize; }
    bool isHovered() const { return hovered; }
private:
    RenderExtent requestedSize;
    bool hovered = false;
    std::array<std::string, 4> debugLabels = { "Normal", "Roughness", "Metallic", "Depth" };
};
