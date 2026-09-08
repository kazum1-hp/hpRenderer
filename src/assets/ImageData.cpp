#include "hpr/assets/ImageData.h"
#include "stb_image.h"
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <iostream>

ImageData ImageData::Load(const std::string& path, bool floatingPoint)
{
    std::ifstream stream(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
    if (!stream)
    {
        std::cerr << "Image not found: " << path << '\n';
        return {};
    }
    const auto size = stream.tellg();
    if (size <= 0 || size > std::numeric_limits<int>::max())
        return {};
    std::vector<unsigned char> bytes(static_cast<size_t>(size));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size)))
        return {};
    return Decode(bytes, floatingPoint);
}
ImageData ImageData::Decode(const std::vector<unsigned char>& bytes, bool floatingPoint)
{
    if (bytes.empty() || bytes.size() > std::numeric_limits<int>::max())
        return {};
    ImageData image;
    int channels;
    // Thread-local policy: preparing an HDR must not change another import's UV orientation.
    stbi_set_flip_vertically_on_load_thread(floatingPoint);
    void* pixels = floatingPoint
                       ? static_cast<void*>(stbi_loadf_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                                                   &image.width, &image.height, &channels, 4))
                       : static_cast<void*>(stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                                                  &image.width, &image.height, &channels, 4));
    std::unique_ptr<void, decltype(&stbi_image_free)> owner(pixels, stbi_image_free);
    if (!pixels)
    {
        std::cerr << "Image decode failed: " << stbi_failure_reason() << '\n';
        return {};
    }
    const size_t count = static_cast<size_t>(image.width) * image.height * 4;
    if (floatingPoint)
        image.hdr.assign(static_cast<float*>(pixels), static_cast<float*>(pixels) + count);
    else
        image.rgba.assign(static_cast<unsigned char*>(pixels), static_cast<unsigned char*>(pixels) + count);
    return image;
}
