#pragma once
#include "hpr/renderer/RendererStatistics.h"
#include <memory>

namespace Rendering
{
// Only the active renderer on this thread is instrumented. Calls outside a frame are no-ops.
void BeginGPUQuery(const char* name);
void EndGPUQuery();
void RecordDraw(std::uint64_t triangles);

class ScopedGPUQuery
{
public:
    explicit ScopedGPUQuery(const char* name) { BeginGPUQuery(name); }
    ~ScopedGPUQuery() { EndGPUQuery(); }
    ScopedGPUQuery(const ScopedGPUQuery&) = delete;
    ScopedGPUQuery& operator=(const ScopedGPUQuery&) = delete;
};

// Identity must remain stable during the frame. A scope counts only if a nonempty draw occurs.
class ScopedRenderedObject
{
public:
    explicit ScopedRenderedObject(const void* identity);
    ~ScopedRenderedObject();
    ScopedRenderedObject(const ScopedRenderedObject&) = delete;
    ScopedRenderedObject& operator=(const ScopedRenderedObject&) = delete;
private:
    const void* previous;
};

class RenderProfiler
{
public:
    RenderProfiler();
    ~RenderProfiler(); // Release queries while the owning GL context is current.
    RenderProfiler(const RenderProfiler&) = delete;
    RenderProfiler& operator=(const RenderProfiler&) = delete;
    void beginFrame();
    void endFrame();
    void shutdown();
    const RendererStatistics& statistics() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    friend void BeginGPUQuery(const char*);
    friend void EndGPUQuery();
    friend void RecordDraw(std::uint64_t);
};
class ScopedRenderFrame
{
public:
    explicit ScopedRenderFrame(RenderProfiler& profiler) : profiler(profiler) { profiler.beginFrame(); }
    ~ScopedRenderFrame() { profiler.endFrame(); }
    ScopedRenderFrame(const ScopedRenderFrame&) = delete;
    ScopedRenderFrame& operator=(const ScopedRenderFrame&) = delete;
private:
    RenderProfiler& profiler;
};
}
