#include "hpr/renderer/opengl/RenderProfiler.h"
#include <glad/glad.h>
#include <array>
#include <chrono>
#include <stdexcept>
#include <unordered_set>

namespace Rendering
{
namespace
{
thread_local RenderProfiler* active = nullptr;
thread_local const void* objectIdentity = nullptr;
constexpr std::size_t MaxQueries = 128;
using Clock = std::chrono::steady_clock;
}
struct RenderProfiler::Impl
{
    struct Slot
    {
        std::array<GLuint, MaxQueries * 2> ids{};
        std::vector<GPUQueryResult> queries;
        std::uint64_t frame = 0;
        bool pending = false;
    };
    std::array<Slot, 4> slots;
    Slot* recording = nullptr;
    std::vector<std::size_t> stack;
    std::unordered_set<const void*> objects;
    RendererStatistics stats;
    Clock::time_point start;
    bool checkedSupport = false;
    bool overflow = false;

    void collect()
    {
        for (auto& slot : slots)
        {
            if (!slot.pending) continue;
            // Frame's closing timestamp is issued after every child timestamp.
            GLint ready = GL_FALSE;
            glGetQueryObjectiv(slot.ids[1], GL_QUERY_RESULT_AVAILABLE, &ready);
            if (!ready) continue; // Never wait for a GPU result.
            if (slot.frame > stats.gpuFrameNumber)
            {
                for (std::size_t i = 0; i < slot.queries.size(); ++i)
                {
                    GLuint64 begin = 0, end = 0;
                    glGetQueryObjectui64v(slot.ids[i * 2], GL_QUERY_RESULT, &begin);
                    glGetQueryObjectui64v(slot.ids[i * 2 + 1], GL_QUERY_RESULT, &end);
                    slot.queries[i].milliseconds = static_cast<double>(end - begin) / 1000000.0;
                }
                stats.gpuQueries = slot.queries;
                stats.gpuFrameNumber = slot.frame;
                stats.gpuValid = true;
            }
            slot.pending = false;
        }
    }
};

RenderProfiler::RenderProfiler() : impl(std::make_unique<Impl>()) {}
RenderProfiler::~RenderProfiler() { shutdown(); }
const RendererStatistics& RenderProfiler::statistics() const { return impl->stats; }
void RenderProfiler::shutdown()
{
    if (active == this) { active = nullptr; objectIdentity = nullptr; }
    for (auto& slot : impl->slots)
    {
        if (slot.ids[0]) glDeleteQueries(static_cast<GLsizei>(slot.ids.size()), slot.ids.data());
        slot = {};
    }
    impl->recording = nullptr;
    impl->stack.clear();
    impl->objects.clear();
    impl->stats = {};
    impl->checkedSupport = false;
}
void RenderProfiler::beginFrame()
{
    if (active) throw std::logic_error("RenderProfiler frames cannot overlap on one thread");
    auto& p = *impl;
    p.start = Clock::now();
    if (!p.checkedSupport)
    {
        GLint bits = 0;
        if (glQueryCounter && glGetQueryObjectui64v) glGetQueryiv(GL_TIMESTAMP, GL_QUERY_COUNTER_BITS, &bits);
        p.stats.gpuSupported = bits > 0;
        p.checkedSupport = true;
    }
    p.collect();
    ++p.stats.frameNumber;
    p.stats.drawCalls = p.stats.triangles = p.stats.renderedObjects = 0;
    p.objects.clear();
    p.stack.clear();
    p.overflow = false;
    p.recording = nullptr;
    if (p.stats.gpuSupported)
    {
        for (auto& slot : p.slots)
            if (!slot.pending)
            {
                if (!slot.ids[0]) glGenQueries(static_cast<GLsizei>(slot.ids.size()), slot.ids.data());
                slot.queries.clear();
                slot.frame = p.stats.frameNumber;
                p.recording = &slot;
                break;
            }
        if (!p.recording) ++p.stats.skippedGPUFrames;
    }
    active = this;
    objectIdentity = nullptr;
    BeginGPUQuery("Frame");
}
void RenderProfiler::endFrame()
{
    auto& p = *impl;
    while (!p.stack.empty()) EndGPUQuery();
    if (p.recording)
    {
        // On overflow discard the whole timing sample, rather than publish incomplete data.
        p.recording->pending = true;
        if (p.overflow) { p.recording->frame = 0; ++p.stats.skippedGPUFrames; }
    }
    p.recording = nullptr;
    p.stats.cpuFrameMs = std::chrono::duration<double, std::milli>(Clock::now() - p.start).count();
    active = nullptr;
    objectIdentity = nullptr;
}
void BeginGPUQuery(const char* name)
{
    if (!active) return;
    auto& p = *active->impl;
    std::size_t index = MaxQueries;
    if (p.recording && p.recording->queries.size() < MaxQueries)
    {
        index = p.recording->queries.size();
        GPUQueryResult result;
        result.name = name ? name : "Unnamed Pass";
        result.depth = static_cast<unsigned>(p.stack.size());
        p.recording->queries.push_back(std::move(result));
        glQueryCounter(p.recording->ids[index * 2], GL_TIMESTAMP);
    }
    else if (p.recording) p.overflow = true;
    p.stack.push_back(index);
}
void EndGPUQuery()
{
    if (!active) return;
    auto& p = *active->impl;
    if (p.stack.empty()) return;
    const auto index = p.stack.back();
    p.stack.pop_back();
    if (p.recording && index < MaxQueries)
        glQueryCounter(p.recording->ids[index * 2 + 1], GL_TIMESTAMP);
}
void RecordDraw(std::uint64_t triangles)
{
    if (!active) return;
    auto& p = *active->impl;
    ++p.stats.drawCalls;
    p.stats.triangles += triangles;
    if (triangles && objectIdentity && p.objects.insert(objectIdentity).second) ++p.stats.renderedObjects;
    if (p.recording)
        for (const auto index : p.stack)
            if (index < MaxQueries)
            {
                ++p.recording->queries[index].drawCalls;
                p.recording->queries[index].triangles += triangles;
            }
}
ScopedRenderedObject::ScopedRenderedObject(const void* identity) : previous(objectIdentity) { objectIdentity = identity; }
ScopedRenderedObject::~ScopedRenderedObject() { objectIdentity = previous; }
}
