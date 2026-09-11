#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Rendering
{
struct DrawStatistics
{
    std::uint64_t drawCalls = 0;
    std::uint64_t triangles = 0;
};
struct GPUQueryResult : DrawStatistics
{
    std::string name;
    double milliseconds = 0;
    unsigned depth = 0;
};
struct RendererStatistics : DrawStatistics
{
    std::uint64_t frameNumber = 0;
    double cpuFrameMs = 0; // Renderer::render wall time, including driver work.
    std::uint64_t renderedObjects = 0; // Unique objects submitted in camera passes, including ground.
    bool gpuSupported = false;
    bool gpuValid = false;
    std::uint64_t gpuFrameNumber = 0;
    std::uint64_t skippedGPUFrames = 0;
    std::vector<GPUQueryResult> gpuQueries; // One complete, possibly older frame; first entry is Frame.
};
}
