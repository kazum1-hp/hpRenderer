#pragma once

#include "RenderTypes.h"
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

enum class ColorFormat { RGB8, RGBA16F };
enum class TextureFilter { Nearest, Linear };
enum class DepthStorage { Texture2D, Cubemap, Renderbuffer };
// Depth preserves the implementation-selected precision used by existing shadows.
enum class DepthFormat { Depth, Depth24, Depth24Stencil8 };
enum class DepthWrap { ClampToEdge, ClampToBorder };

struct ColorAttachmentDesc
{
    ColorFormat format = ColorFormat::RGB8;
    TextureFilter filter = TextureFilter::Linear;
};

struct DepthAttachmentDesc
{
    DepthStorage storage = DepthStorage::Renderbuffer;
    DepthFormat format = DepthFormat::Depth24Stencil8;
    DepthWrap wrap = DepthWrap::ClampToEdge;
};

struct FramebufferDesc
{
    static constexpr std::size_t MaxColorAttachments = 8;
    RenderExtent extent;
    std::vector<ColorAttachmentDesc> colors;
    std::optional<DepthAttachmentDesc> depth;
    unsigned int samples = 1;
    std::string debugName;

    // Validate before allocating GL objects; also usable without a GL context.
    void validate() const
    {
        const auto fail = [this](const char* message) {
            throw std::invalid_argument("Framebuffer '" + debugName + "': " + message);
        };
        if (!extent.isValid() || extent.width > static_cast<unsigned int>((std::numeric_limits<int>::max)()) ||
            extent.height > static_cast<unsigned int>((std::numeric_limits<int>::max)()))
            fail("extent must be positive and fit GLsizei");
        if (colors.size() > MaxColorAttachments) fail("too many color attachments");
        if (colors.empty() && !depth) fail("at least one attachment is required");
        for (const auto& color : colors)
        {
            if (color.format != ColorFormat::RGB8 && color.format != ColorFormat::RGBA16F)
                fail("unsupported color format");
            if (color.filter != TextureFilter::Nearest && color.filter != TextureFilter::Linear)
                fail("unsupported color filter");
        }
        if (depth)
        {
            if (depth->wrap != DepthWrap::ClampToEdge && depth->wrap != DepthWrap::ClampToBorder)
                fail("unsupported depth wrap mode");
            switch (depth->storage)
            {
            case DepthStorage::Renderbuffer:
                if (depth->format != DepthFormat::Depth24Stencil8)
                    fail("renderbuffer requires Depth24Stencil8");
                if (depth->wrap != DepthWrap::ClampToEdge)
                    fail("renderbuffer has no texture wrap mode");
                break;
            case DepthStorage::Texture2D:
            case DepthStorage::Cubemap:
                if (depth->format != DepthFormat::Depth && depth->format != DepthFormat::Depth24)
                    fail("depth texture requires Depth or Depth24");
                break;
            default:
                fail("unsupported depth storage");
            }
            if (depth->storage == DepthStorage::Cubemap)
            {
                if (extent.width != extent.height) fail("cubemap faces must be square");
                if (!colors.empty()) fail("layered depth cubemap cannot share non-layered color attachments");
            }
        }
        // Scope of this wrapper, not an OpenGL limitation: retain the old 4x RGB8 path.
        if (samples != 1)
        {
            if (samples != 4 || colors.size() != 1 || colors.front().format != ColorFormat::RGB8 ||
                (depth && depth->storage != DepthStorage::Renderbuffer))
                fail("multisampling supports only 4x single RGB8 color with optional depth renderbuffer");
        }
    }
};
