#pragma once
#include <string>
#include <vector>
#include <limits>

// Decoded CPU pixels. Always RGBA; HDR uses floats, other images use bytes.
struct ImageData
{
    int width = 0, height = 0;
    std::vector<unsigned char> rgba;
    std::vector<float> hdr;
    bool isHDR() const
    {
        return !hdr.empty();
    }
    bool valid() const
    {
        if (width <= 0 || height <= 0 ||
            static_cast<size_t>(width) > std::numeric_limits<size_t>::max() / 4 / static_cast<size_t>(height))
            return false;
        const auto count = static_cast<size_t>(width) * height * 4;
        return (rgba.size() == count && hdr.empty()) || (hdr.size() == count && rgba.empty());
    }
    static ImageData Load(const std::string& path, bool floatingPoint = false);
    static ImageData Decode(const std::vector<unsigned char>& encoded, bool floatingPoint = false);
};
