#include "hpr/editor/panels/Panels.h"
#include "hpr/scene/Scene.h"
#include "hpr/assets/AssetManager.h"
#include "imgui.h"
#include "ImGuiFileDialog.h"
#include <cstdio>

static inline void SafeCopyPath(char* dest, size_t destSize, const std::string& src)
{
    if (dest == nullptr || destSize == 0) return;
    // Use snprintf to perform a safe copy and ensure null-terminate
    std::snprintf(dest, destSize, "%s", src.c_str());
    dest[destSize - 1] = '\0';
}

static inline std::string FileNameFromPath(const std::string& path)
{
    const size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}


AssetPanel::AssetPanel(AssetManager& resources) : resources(resources), fileDialog(std::make_unique<IGFD::FileDialog>()) {}
AssetPanel::~AssetPanel() = default;

bool AssetPanel::draw(Scene& scene, unsigned int dialogDockId)
{
    bool shadersReloaded = false;
    ImGui::Begin("Reload Shaders");
    if (ImGui::Button("Reload Shaders"))
    {
        lastReloadMsg.clear();
        for (const auto& result : resources.ReloadAllShaders())
        {
            lastReloadMsg += std::string(ShaderName(result.first)) + (result.second ? ": reloaded\n" : ": no change or failed\n");
            shadersReloaded = shadersReloaded || result.second;
        }
    }
    ImGui::Separator();
    ImGui::TextWrapped("%s", lastReloadMsg.c_str());
    ImGui::End();

    // ------------------- Hot Reload Assets ----------------------
    ImGui::Begin("Reload Assets");
    ImGui::TextWrapped("Static models: glTF / GLB / OBJ / PMX / FBX. Convert .blend in Blender first.");

    auto loadModelFromPath = [&](const std::string& path) {
        auto& res = resources;
        auto mdl = res.LoadModel(path);
        if (!mdl || !mdl->isValid())
        {
            hotReloadMsg = "Failed to load model: " + path;
            return std::shared_ptr<Model>();
        }
        return mdl;
    };

    auto assetObjectLabel = [&](int index) {
        const auto& currentObjects = scene.GetObjects();
        if (currentObjects.empty()) return std::string("No Object");

        const auto& object = currentObjects[static_cast<size_t>(index)];
        if (object.model && !object.model->getPath().empty())
        {
            return "Object " + std::to_string(index) + " - " + FileNameFromPath(object.model->getPath());
        }

        return "Object " + std::to_string(index);
    };

    auto replaceAssetSelectedModel = [&](const std::string& path) {
        auto mdl = loadModelFromPath(path);
        if (!mdl) return;

        auto& currentObjects = scene.GetObjects();
        if (currentObjects.empty())
        {
            scene.AddObject(mdl);
            assetSelectedIndex = 0;
            hotReloadMsg = "Model added: " + path;
            return;
        }

        if (assetSelectedIndex < 0) assetSelectedIndex = 0;
        if (assetSelectedIndex >= static_cast<int>(currentObjects.size()))
            assetSelectedIndex = static_cast<int>(currentObjects.size()) - 1;

        currentObjects[static_cast<size_t>(assetSelectedIndex)].model = mdl;
        hotReloadMsg = "Replaced selected model: Object " + std::to_string(assetSelectedIndex) + " <- " + path;
    };

    auto addAssetModel = [&](const std::string& path) {
        auto mdl = loadModelFromPath(path);
        if (!mdl) return;

        scene.AddObject(mdl);
        assetSelectedIndex = static_cast<int>(scene.GetObjects().size()) - 1;
        hotReloadMsg = "Model added: " + path;
    };

    auto runPendingModelAction = [&](const std::string& path) {
        if (pendingModelAction == 1)
        {
            replaceAssetSelectedModel(path);
            pendingModelAction = 0;
        }
        else if (pendingModelAction == 2)
        {
            addAssetModel(path);
            pendingModelAction = 0;
        }
    };

    auto& assetObjects = scene.GetObjects();
    if (assetObjects.empty())
    {
        assetSelectedIndex = 0;
        ImGui::Text("No model objects in scene.");
    }
    else
    {
        if (assetSelectedIndex < 0) assetSelectedIndex = 0;
        if (assetSelectedIndex >= static_cast<int>(assetObjects.size()))
            assetSelectedIndex = static_cast<int>(assetObjects.size()) - 1;

        const std::string assetModelName = assetObjectLabel(assetSelectedIndex);
        if (ImGui::BeginCombo("Select Model", assetModelName.c_str()))
        {
            for (int i = 0; i < static_cast<int>(assetObjects.size()); ++i)
            {
                const std::string label = assetObjectLabel(i);
                const bool isSelected = assetSelectedIndex == i;
                if (ImGui::Selectable(label.c_str(), isSelected))
                {
                    assetSelectedIndex = i;
                    pendingModelAction = 0;
                    auto selectedModel = assetObjects[static_cast<size_t>(assetSelectedIndex)].model;
                    if (selectedModel && !selectedModel->getPath().empty())
                    {
                        SafeCopyPath(modelPathBuf, sizeof(modelPathBuf), selectedModel->getPath());
                    }
                }

                if (isSelected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
    }

    if (ImGui::Button("Reload Selected Model")) {
        std::string path;
        if (assetObjects.empty() ||
            assetSelectedIndex < 0 ||
            assetSelectedIndex >= static_cast<int>(assetObjects.size()))
        {
            hotReloadMsg = "No selected model to reload.";
        }
        else if (assetObjects[static_cast<size_t>(assetSelectedIndex)].model)
        {
            auto selectedModel = assetObjects[static_cast<size_t>(assetSelectedIndex)].model;
            path = selectedModel->getPath();

            bool ok = !path.empty() && selectedModel->reload(path);
            hotReloadMsg = ok
                ? ("Reloaded selected model: Object " + std::to_string(assetSelectedIndex) + " <- " + path)
                : ("Reload selected model failed: " + path);
        }
        else
        {
            hotReloadMsg = "Selected object has no model to reload.";
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Delete Selected Model")) {
        if (assetObjects.empty() ||
            assetSelectedIndex < 0 ||
            assetSelectedIndex >= static_cast<int>(assetObjects.size()))
        {
            hotReloadMsg = "No selected model to delete.";
        }
        else
        {
            const int deletedIndex = assetSelectedIndex;
            if (scene.RemoveObject(static_cast<size_t>(assetSelectedIndex)))
            {
                if (assetSelectedIndex >= static_cast<int>(scene.GetObjects().size()))
                    assetSelectedIndex = static_cast<int>(scene.GetObjects().size()) - 1;
                if (assetSelectedIndex < 0) assetSelectedIndex = 0;
                pendingModelAction = 0;
                hotReloadMsg = "Deleted model object: Object " + std::to_string(deletedIndex);
            }
            else
            {
                hotReloadMsg = "Delete selected model failed.";
            }
        }
    }

    if (ImGui::Button("Replace Model...")) {
        pendingModelAction = 1;
        if (!assetObjects.empty() &&
            assetSelectedIndex >= 0 &&
            assetSelectedIndex < static_cast<int>(assetObjects.size()))
        {
            auto selectedModel = assetObjects[static_cast<size_t>(assetSelectedIndex)].model;
            if (selectedModel && !selectedModel->getPath().empty())
            {
                SafeCopyPath(modelPathBuf, sizeof(modelPathBuf), selectedModel->getPath());
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Add Model...")) {
        pendingModelAction = 2;
    }

    if (pendingModelAction != 0)
    {
        ImGui::Separator();
        ImGui::Text("%s", pendingModelAction == 1 ? "Replace selected model" : "Add model");
        ImGui::InputText("Model Path", modelPathBuf, sizeof(modelPathBuf));

        if (ImGui::Button("Browse Model...")) {
            IGFD::FileDialogConfig cfg;
            cfg.path = "../assets/models";
            cfg.flags |= ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;
            fileDialog->OpenDialog("ChooseModelDlg", "Choose Model", "Models{.gltf,.glb,.obj,.pmx,.fbx}", cfg);
        }

        if (fileDialog->IsOpened("ChooseModelDlg"))
            ImGui::SetNextWindowDockID(dialogDockId, ImGuiCond_FirstUseEver);
        if (fileDialog->Display("ChooseModelDlg")) {
            if (fileDialog->IsOk()) {
                std::string chosen = fileDialog->GetFilePathName();
                SafeCopyPath(modelPathBuf, sizeof(modelPathBuf), chosen);
                modelPathBuf[sizeof(modelPathBuf)-1] = '\0';
                runPendingModelAction(chosen);
            }
            fileDialog->Close();
        }
    }
    else
    {
        if (fileDialog->Display("ChooseModelDlg"))
        {
            fileDialog->Close();
        }
    }

    ImGui::Separator();

    ImGui::InputText("HDR Path", hdrPathBuf, sizeof(hdrPathBuf));
    //ImGui::SameLine();
    if (ImGui::Button("Browse HDR...")) {
        IGFD::FileDialogConfig cfg2;
        cfg2.path = "../assets/hdr";
        fileDialog->OpenDialog("ChooseHdrDlg", "Choose HDR", ".hdr", cfg2);
    }

    if (fileDialog->IsOpened("ChooseHdrDlg"))
        ImGui::SetNextWindowDockID(dialogDockId, ImGuiCond_FirstUseEver);

    if (fileDialog->Display("ChooseHdrDlg")) {
        if (fileDialog->IsOk()) {
            std::string chosen = fileDialog->GetFilePathName();
            SafeCopyPath(hdrPathBuf, sizeof(hdrPathBuf), chosen);
            hdrPathBuf[sizeof(hdrPathBuf)-1] = '\0';

            auto& res = resources;
            auto asset = res.LoadEnvironment(chosen);
            if (asset) {
                scene.SetEnvironment(asset);
                hotReloadMsg = "HDR loaded: " + chosen;
            } else {
                hotReloadMsg = "Failed to load HDR: " + chosen;
            }
        }
        fileDialog->Close();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reload HDR (same path)")) {
        std::string path(hdrPathBuf);
        auto asset = resources.ReloadEnvironment(path);
        if (asset) {
            scene.SetEnvironment(asset);
            hotReloadMsg = "Reloaded HDR: " + path;
        } else {
            hotReloadMsg = "Reload HDR failed: " + path;
        }
    }

    ImGui::NewLine();
    ImGui::SeparatorText("Environment");
    auto& environment = scene.GetEnvironment();
    auto& lighting = environment.lighting;
    int lightingMode = static_cast<int>(lighting.mode);
    if (ImGui::Combo("Ambient Lighting", &lightingMode, "None\0Hemisphere\0IBL\0"))
        lighting.mode = static_cast<AmbientLightingMode>(lightingMode);
    if (lighting.mode == AmbientLightingMode::Hemisphere)
    {
        ImGui::SliderFloat("Ambient Intensity", &lighting.hemisphereIntensity, 0.0f, 2.0f);
        ImGui::ColorEdit3("Sky Color", &lighting.skyColor.x);
        ImGui::ColorEdit3("Ground Color", &lighting.groundColor.x);
        ImGui::TextWrapped("Lights surfaces without an HDR image. Keep this subtle to preserve sun shadow contrast.");
    }
    else if (lighting.mode == AmbientLightingMode::IBL)
    {
        ImGui::SliderFloat("IBL Intensity", &lighting.iblIntensity, 0.0f, 5.0f);
        if (environment.mode != EnvironmentMode::IBL || !environment.asset)
            ImGui::TextWrapped("IBL needs an HDR environment: choose HDR below and load an HDR image.");
    }
    int environmentMode = static_cast<int>(environment.mode);
    if (ImGui::Combo("Environment Mode", &environmentMode, "HDR\0Six Images\0Disabled\0"))
        environment.mode = static_cast<EnvironmentMode>(environmentMode);
    if (environment.mode == EnvironmentMode::SixFaces)
    {
        ImGui::TextWrapped("Background only; choose Hemisphere above for ambient lighting. Face order: +X -X +Y -Y +Z -Z.");
        ImGui::TextWrapped("Named files: +X=px, -X=nx, +Y=py, -Y=ny, +Z=pz, -Z=nz. Camera forward is normally -Z.");
        const char* labels[] = {"+X Right", "-X Left", "+Y Top", "-Y Bottom", "+Z Front", "-Z Back"};
        for (int i = 0; i < 6; ++i)
        {
            ImGui::PushID(i);
            ImGui::InputText(labels[i], skyboxPaths[i].data(), skyboxPaths[i].size());
            ImGui::SameLine();
            if (ImGui::Button("Browse"))
            {
                pendingSkyboxFace = i;
                IGFD::FileDialogConfig cfg;
                cfg.path = "../assets";
                cfg.flags |= ImGuiFileDialogFlags_CaseInsensitiveExtentionFiltering;
                fileDialog->OpenDialog("ChooseSkyboxFace", "Choose skybox image", "Images{.png,.jpg,.jpeg,.tga,.bmp}", cfg);
            }
            ImGui::PopID();
        }
        if (ImGui::Button("Match px/nx/py/ny/pz/nz"))
        {
            std::array<std::string, 6> paths, ordered;
            for (int i = 0; i < 6; ++i) paths[i] = skyboxPaths[i].data();
            try
            {
                if (SkyboxAsset::MatchNamedFaces(paths, ordered, hotReloadMsg))
                {
                    for (int i = 0; i < 6; ++i)
                        SafeCopyPath(skyboxPaths[i].data(), skyboxPaths[i].size(), ordered[i]);
                    hotReloadMsg = "Face paths matched. Click Load / Reload Six Images to apply.";
                }
            }
            catch (const std::exception& error) { hotReloadMsg = error.what(); }
        }
        if (ImGui::Button("Load / Reload Six Images"))
        {
            std::array<std::string, 6> paths;
            for (int i = 0; i < 6; ++i) paths[i] = skyboxPaths[i].data();
            try
            {
                auto replacement = SkyboxAsset::Load(paths, hotReloadMsg);
                if (replacement) { environment.skybox = std::move(replacement); hotReloadMsg = "Six-image skybox loaded."; }
            }
            catch (const std::exception& error) { hotReloadMsg = error.what(); }
        }
        if (!environment.skybox) ImGui::TextWrapped("Select and load all six images. Existing HDR selection is preserved.");
    }
    if (fileDialog->Display("ChooseSkyboxFace"))
    {
        if (fileDialog->IsOk() && pendingSkyboxFace >= 0)
            SafeCopyPath(skyboxPaths[pendingSkyboxFace].data(), skyboxPaths[pendingSkyboxFace].size(), fileDialog->GetFilePathName());
        fileDialog->Close();
        pendingSkyboxFace = -1;
    }
    ImGui::TextWrapped("%s", hotReloadMsg.c_str());
    ImGui::End();


    return shadersReloaded;
}
