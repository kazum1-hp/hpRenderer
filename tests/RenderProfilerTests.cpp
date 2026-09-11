#include "hpr/renderer/opengl/RenderProfiler.h"
#include <glad/glad.h>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

namespace
{
bool ready = false;
GLint counterBits = 64;
GLuint nextId = 1;
unsigned allocated = 0, deleted = 0, reads = 0;
GLuint64 timestamp = 0;
std::unordered_map<GLuint, GLuint64> timestamps;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
void APIENTRY gen(GLsizei count, GLuint* ids)
{
    allocated += count;
    for (GLsizei i = 0; i < count; ++i) ids[i] = nextId++;
}
void APIENTRY remove(GLsizei count, const GLuint*) { deleted += count; }
void APIENTRY bits(GLenum, GLenum, GLint* result) { *result = counterBits; }
void APIENTRY stamp(GLuint id, GLenum) { timestamps[id] = (timestamp += 1000000); }
void APIENTRY available(GLuint, GLenum name, GLint* result)
{
    require(name == GL_QUERY_RESULT_AVAILABLE, "unexpected blocking query");
    *result = ready;
}
void APIENTRY result(GLuint id, GLenum, GLuint64* value)
{
    require(ready, "read result before available");
    ++reads;
    *value = timestamps.at(id);
}
}
int main()
{
    try
    {
        glad_glGenQueries = gen; glad_glDeleteQueries = remove; glad_glGetQueryiv = bits;
        glad_glQueryCounter = stamp; glad_glGetQueryObjectiv = available; glad_glGetQueryObjectui64v = result;
        using namespace Rendering;
        RenderProfiler profiler;
        int object = 0;
        BeginGPUQuery("Outside"); RecordDraw(100); EndGPUQuery();
        for (int frame = 0; frame < 6; ++frame)
        {
            ScopedRenderFrame scope(profiler);
            ScopedGPUQuery pass("Geometry");
            ScopedRenderedObject item(&object);
            RecordDraw(2);
            { ScopedGPUQuery child("Child"); RecordDraw(3); }
            RecordDraw(0);
        }
        const auto& stats = profiler.statistics();
        require(stats.drawCalls == 3 && stats.triangles == 5 && stats.renderedObjects == 1, "counts/reset/dedup failed");
        require(!stats.gpuValid && reads == 0, "pending GPU should not publish or block");
        require(stats.skippedGPUFrames == 2 && allocated == 4 * 128 * 2, "query pool must remain bounded");
        ready = true;
        profiler.beginFrame();
        require(stats.gpuValid && stats.gpuFrameNumber == 4, "latest complete sample not selected");
        require(stats.gpuQueries.size() == 3 && stats.gpuQueries[1].name == "Geometry", "sample labels lost");
        require(stats.gpuQueries[0].milliseconds == 5 && stats.gpuQueries[1].milliseconds == 3 &&
                stats.gpuQueries[2].milliseconds == 1, "nested timestamps converted incorrectly");
        require(stats.gpuQueries[0].triangles == 5 && stats.gpuQueries[1].drawCalls == 3 &&
                stats.gpuQueries[2].triangles == 3, "inclusive per-pass counters incorrect");
        profiler.endFrame();
        require(stats.drawCalls == 0 && stats.renderedObjects == 0, "empty frame retained counters");
        profiler.beginFrame();
        for (int i = 0; i < 140; ++i) { ScopedGPUQuery query("Overflow"); }
        profiler.endFrame();
        const auto lastValid = stats.gpuFrameNumber;
        profiler.beginFrame();
        require(stats.gpuFrameNumber == lastValid && stats.skippedGPUFrames == 3, "overflow published partial sample");
        profiler.endFrame();
        profiler.shutdown(); profiler.shutdown();
        require(allocated == deleted && stats.frameNumber == 0, "shutdown leaked queries or stale stats");
        counterBits = 0;
        { ScopedRenderFrame frame(profiler); RecordDraw(7); }
        require(!stats.gpuSupported && !stats.gpuValid && stats.triangles == 7, "unsupported GPU must retain CPU counters");
        profiler.shutdown();
        counterBits = 64;
        try { ScopedRenderFrame frame(profiler); ScopedGPUQuery query("Exception"); throw 1; } catch (int) {}
        { ScopedRenderFrame frame(profiler); }
        require(stats.gpuValid, "exception left active query/frame");
        profiler.shutdown();
        require(allocated == deleted, "query cleanup failed after reinitialization");
        std::cout << "Render profiler tests passed.\n";
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
