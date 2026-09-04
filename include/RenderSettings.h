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
    bool drawLights = false;
    bool drawGBufferDebug = false;

    bool quadraticAttenuation = true;
    bool msaa = true;
    bool blinnPhong = true;

    GroundPlaneSettings groundPlane;
    PostProcessSettings postProcess;
};
