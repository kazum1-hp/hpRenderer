#pragma once
#include "Editor/ConsoleCapture.h"
#include "RenderTypes.h"
#include <array>
#include <memory>
#include <string>

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
    AssetPanel();
    ~AssetPanel();
    // Returns true when at least one shader was successfully reloaded.
    bool draw(Scene& scene, unsigned int dialogDockId);
private:
    std::unique_ptr<IGFD::FileDialog> fileDialog;
    char modelPathBuf[1024] = "../assets/models/blue_metal_plate_4k.gltf/blue_metal_plate_4k.gltf";
    char hdrPathBuf[1024] = "../assets/hdr/newport_loft.hdr";
    std::string lastReloadMsg = "Idle";
    std::string hotReloadMsg = "Idle";
    int assetSelectedIndex = 0;
    int pendingModelAction = 0; // none, replace selected, add
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
    void refreshDebugLabels();
    RenderExtent requestedExtent() const { return requestedSize; }
    bool isHovered() const { return hovered; }
private:
    RenderExtent requestedSize;
    bool hovered = false;
    std::array<std::string, 4> debugLabels = { "Normal", "Roughness", "Metallic", "Depth" };
};
