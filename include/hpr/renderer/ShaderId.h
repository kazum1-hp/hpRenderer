#pragma once
#include <array>
#include <cstddef>

enum class ShaderId
{
    Model,
    Light,
    ToneMapping,
    EnvironmentCapture,
    DirectionalShadow,
    PointShadow,
    BloomBlur,
    GBuffer,
    DeferredLighting,
    Debug,
    GBufferDebug,
    Skybox,
    Irradiance,
    Prefilter,
    Brdf,
    Count
};
inline constexpr std::array<const char*, static_cast<std::size_t>(ShaderId::Count)> ShaderNames{
    "model",     "light", "scene framebuffer", "skybox",     "dir shadow", "point shadow", "bloomBlur", "gBuffer",
    "lightPass", "debug", "gbuffer debug",     "background", "irradiance", "prefilter",    "brdf"};
constexpr const char* ShaderName(ShaderId id)
{
    const auto index = static_cast<std::size_t>(id);
    return index < ShaderNames.size() ? ShaderNames[index] : "invalid shader";
}
