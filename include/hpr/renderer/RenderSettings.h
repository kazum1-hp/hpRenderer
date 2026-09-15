#pragma once

struct GroundPlaneSettings
{
    bool visible = false;
    bool useNormalMap = false;
    bool useHeightMap = false;
    float heightScale = 0.001f;
};

struct PostProcessSettings
{
    // Disabled: default Reinhard + gamma output, without optional effects.
    bool enabled = false;
    bool hdr = true;
    bool bloom = false;

    int toneMappingMode = 0;
    int effectMode = 0;

    float exposure = 1.0f;
    float bloomSampleDistance = 1.0f;
    float kernelOffset = 500.0f;
    float scanPosition = 0.0f;
};

struct RenderSettings
{
    bool deferred = false;
    bool shadows = false;
    float directionalShadowDistance = 100.0f; // Camera-space distance in world units.
    bool drawLights = false;
    bool drawGBufferDebug = false;

    bool quadraticAttenuation = true;
    bool msaa = true;

    GroundPlaneSettings groundPlane;
    PostProcessSettings postProcess;
};
