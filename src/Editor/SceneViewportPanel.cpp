#include "Editor/Panels.h"
#include "RenderSettings.h"
#include "ResourceManager.h"
#include "imgui.h"
#include <cctype>
#include <fstream>

static inline std::string Trim(const std::string& value)
{
    size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin]))) ++begin;

    size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) --end;

    return value.substr(begin, end - begin);
}

static bool ParseDebugLabelMarker(const std::string& comment, int& index, std::string& label)
{
    constexpr const char* marker = "@debugLabel";
    constexpr size_t markerLength = 11;

    if (comment.compare(0, markerLength, marker) != 0)
    {
        return false;
    }

    std::istringstream stream(comment.substr(markerLength));
    if (!(stream >> index))
    {
        return false;
    }

    std::getline(stream, label);
    label = Trim(label);

    return index >= 0 && index < 4 && !label.empty();
}

static bool TryParseDebugModeIndex(const std::string& line, int& index)
{
    const size_t modePos = line.find("u_DebugMode");
    if (modePos == std::string::npos)
    {
        return false;
    }

    const size_t equalsPos = line.find("==", modePos);
    if (equalsPos == std::string::npos)
    {
        return false;
    }

    size_t numberPos = equalsPos + 2;
    while (numberPos < line.size() && std::isspace(static_cast<unsigned char>(line[numberPos]))) ++numberPos;

    if (numberPos >= line.size() || !std::isdigit(static_cast<unsigned char>(line[numberPos])))
    {
        return false;
    }

    index = line[numberPos] - '0';
    return index >= 0 && index < 4;
}


void SceneViewportPanel::refreshDebugLabels()
{
    debugLabels = { "Normal", "Roughness", "Metallic", "Depth" };

    const auto gbufferDebugShader = ResourceManager::GetInstance().GetShader("gbuffer debug");
    if (!gbufferDebugShader)
    {
        return;
    }

    std::ifstream file(gbufferDebugShader->GetFragmentPath());
    if (!file.is_open())
    {
        std::cerr << "WARN::DRAW_DEBUG_LABELS::FILE_NOT_READ: "
                  << gbufferDebugShader->GetFragmentPath() << std::endl;
        return;
    }

    std::array<bool, 4> found = { false, false, false, false };
    int pendingMode = -1;
    std::string line;

    while (std::getline(file, line))
    {
        const size_t commentPos = line.find("//");
        if (commentPos != std::string::npos)
        {
            const std::string comment = Trim(line.substr(commentPos + 2));
            int markerIndex = -1;
            std::string markerLabel;
            if (ParseDebugLabelMarker(comment, markerIndex, markerLabel))
            {
                debugLabels[static_cast<size_t>(markerIndex)] = markerLabel;
                found[static_cast<size_t>(markerIndex)] = true;
                pendingMode = -1;
                continue;
            }

            if (pendingMode >= 0 && !found[static_cast<size_t>(pendingMode)] && !comment.empty())
            {
                debugLabels[static_cast<size_t>(pendingMode)] = comment;
                found[static_cast<size_t>(pendingMode)] = true;
                pendingMode = -1;
                continue;
            }
        }

        int modeIndex = -1;
        if (TryParseDebugModeIndex(line, modeIndex))
        {
            pendingMode = modeIndex;
            continue;
        }

        const std::string trimmed = Trim(line);
        if (pendingMode >= 0 && !trimmed.empty() && trimmed != "{")
        {
            pendingMode = -1;
        }
    }
}


void SceneViewportPanel::draw(const RenderOutput& renderOutput, const RenderSettings& settings)
{
    // ------------ Scene ----------------
    if (!ImGui::Begin("Scene"))
    {
        hovered = false;
        ImGui::End();
        return;
    }
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

    hovered = ImGui::IsWindowHovered();
    ImVec2 size = ImGui::GetContentRegionAvail();

    // A collapsed/tiny panel must never turn a negative float into a huge unsigned size.
    if (size.x <= 0.0f || size.y <= 0.0f)
    {
        hovered = false;
        ImGui::End();
        return;
    }
    if (!ImGui::IsMouseDown(0))
        requestedSize = {static_cast<std::uint32_t>(size.x), static_cast<std::uint32_t>(size.y)};

    if (renderOutput.colorTexture != 0)
    {
        ImVec2 imagePos = ImGui::GetCursorScreenPos();

        ImGui::Image(
            (ImTextureID)(intptr_t)renderOutput.colorTexture,
            size,
            ImVec2(0, 1),   // flip vertically
            ImVec2(1, 0)
        );
        if (settings.drawGBufferDebug && settings.deferred)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            float debugH = size.y / 4.0f;
            float padding = 6.0f;

            for (int i = 0; i < 4; i++)
            {
                float x = imagePos.x + 10.0f;
                float y = imagePos.y + i * debugH + 10.0f;
                const char* label = debugLabels[static_cast<size_t>(i)].c_str();
                ImVec2 textSize = ImGui::CalcTextSize(label);

                // background (clearer/readable)
                ImVec2 bgMin(x - padding, y - padding);
                ImVec2 bgMax(x + textSize.x + padding, y + textSize.y + padding);

                drawList->AddRectFilled(
                    bgMin,
                    bgMax,
                    IM_COL32(0, 0, 0, 150)
                );

                // text
                drawList->AddText(
                    ImVec2(x, y),
                    IM_COL32(255, 255, 0, 255),
                    label
                );
            }
        }
    }
    else
    {
        ImGui::Text("No Render Output");
    }

    ImGui::End();
}
