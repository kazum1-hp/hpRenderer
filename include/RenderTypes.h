#pragma once

#include <cstdint>

struct RenderExtent
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    bool isValid() const
    {
        return width > 0 && height > 0;
    }
};

struct RenderOutput
{
    // Non-owning: resize/shutdown invalidates the texture; later renders may overwrite its pixels.
    unsigned int colorTexture = 0;
    RenderExtent extent;
};
