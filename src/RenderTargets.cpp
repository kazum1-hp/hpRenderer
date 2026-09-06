#include "RenderTargets.h"
#include "RenderLimits.h"
#include <algorithm>
#include <utility>

void RenderTargets::initialize(RenderExtent extent, unsigned int shadowSize, ColorFormat sceneColor)
{
    RenderTargets replacement;
    FramebufferDesc desc;
    desc.extent = extent;
    desc.colors = { {sceneColor}, {sceneColor} };
    desc.depth = DepthAttachmentDesc{}; // Depth24Stencil8 renderbuffer
    desc.debugName = "HDR Target";
    replacement.hdr = std::make_unique<FrameBuffer>(desc);

    desc.extent = { shadowSize, shadowSize };
    desc.colors.clear();
    desc.depth = { DepthStorage::Texture2D, DepthFormat::Depth, DepthWrap::ClampToBorder };
    desc.debugName = "Directional Shadow";
    replacement.directionalShadow = std::make_unique<FrameBuffer>(desc);

    desc.extent = extent;
    desc.colors.assign(5, { ColorFormat::RGBA16F, TextureFilter::Nearest });
    desc.depth = { DepthStorage::Texture2D, DepthFormat::Depth24 };
    desc.debugName = "G-buffer";
    replacement.gbuffer = std::make_unique<FrameBuffer>(desc);

    desc.colors = { {sceneColor}, {sceneColor} };
    desc.depth = DepthAttachmentDesc{};
    desc.debugName = "Deferred Lighting";
    replacement.deferredLighting = std::make_unique<FrameBuffer>(desc);

    desc.colors = { {ColorFormat::RGB8} };
    desc.debugName = "Final Output";
    replacement.finalOutput = std::make_unique<FrameBuffer>(desc);

    desc.colors = { {ColorFormat::RGBA16F} };
    // Preserve the depth texture allocated by the old useDepth=false path.
    desc.depth = { DepthStorage::Texture2D, DepthFormat::Depth24 };
    for (std::size_t i = 0; i < replacement.bloomPingPong.size(); ++i)
    {
        desc.debugName = "Bloom Ping-Pong " + std::to_string(i);
        replacement.bloomPingPong[i] = std::make_unique<FrameBuffer>(desc);
    }
    *this = std::move(replacement);
}

void RenderTargets::resizeViewport(RenderExtent extent)
{
    // Allocate the complete viewport set before releasing any of the old targets.
    const auto resized = [extent](const std::unique_ptr<FrameBuffer>& target) {
        auto desc = target->getDesc();
        desc.extent = extent;
        return std::make_unique<FrameBuffer>(std::move(desc));
    };
    if (hdr->getWidth() == extent.width && hdr->getHeight() == extent.height) return;
    auto newHdr = resized(hdr);
    auto newGbuffer = resized(gbuffer);
    auto newLighting = resized(deferredLighting);
    auto newOutput = resized(finalOutput);
    std::array<std::unique_ptr<FrameBuffer>, 2> newBloom;
    for (std::size_t i = 0; i < newBloom.size(); ++i) newBloom[i] = resized(bloomPingPong[i]);
    hdr = std::move(newHdr);
    gbuffer = std::move(newGbuffer);
    deferredLighting = std::move(newLighting);
    finalOutput = std::move(newOutput);
    bloomPingPong = std::move(newBloom);
}

void RenderTargets::syncPointShadows(std::size_t count, unsigned int shadowSize)
{
    count = std::min(count, RenderLimits::MaxPointLights);
    pointShadows.resize(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (pointShadows[i]) continue;
        FramebufferDesc desc;
        desc.extent = { shadowSize, shadowSize };
        desc.depth = { DepthStorage::Cubemap, DepthFormat::Depth };
        desc.debugName = "Point Shadow " + std::to_string(i);
        pointShadows[i] = std::make_unique<FrameBuffer>(std::move(desc));
    }
}
