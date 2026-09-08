#pragma once

// Imported model materials remain authoritative; only these per-object adjustments are editable.
struct MaterialInstance
{
    float aoBias = 0.0f;
    float roughnessBias = 0.0f;
    float metallicBias = 0.0f;
    bool useNormalMap = true;
};
