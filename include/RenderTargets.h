#pragma once

#include "FrameBuffer.h"
#include <array>
#include <memory>
#include <vector>

// Owns the pipeline's GPU targets. Construct/clear while the GL context is current.
struct RenderTargets
{
    std::unique_ptr<FrameBuffer> hdr;
    std::unique_ptr<FrameBuffer> directionalShadow;
    std::unique_ptr<FrameBuffer> gbuffer;
    std::unique_ptr<FrameBuffer> deferredLighting;
    std::unique_ptr<FrameBuffer> finalOutput;
    std::array<std::unique_ptr<FrameBuffer>, 2> bloomPingPong;
    std::vector<std::unique_ptr<FrameBuffer>> pointShadows;

    void initialize(RenderExtent extent, unsigned int shadowSize, ColorFormat sceneColor);
    void resizeViewport(RenderExtent extent);
    void syncPointShadows(std::size_t count, unsigned int shadowSize);
};
