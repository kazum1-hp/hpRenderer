#include "RenderTargets.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <iostream>
#include <utility>

namespace
{
    void require(bool condition, const char* message)
    {
        if (!condition) throw std::runtime_error(message);
    }

    template<class Action> void rejects(Action action)
    {
        try { action(); }
        catch (const std::invalid_argument&) { return; }
        throw std::runtime_error("invalid framebuffer configuration was accepted");
    }

    void descriptorTests()
    {
        FramebufferDesc desc;
        desc.debugName = "Validation test";
        rejects([&] { desc.validate(); });
        desc.extent = {64, 64};
        rejects([&] { desc.validate(); }); // no attachments
        desc.colors = {{ColorFormat::RGBA16F}};
        desc.validate(); // genuinely color-only, no implicit depth
        desc.colors.resize(FramebufferDesc::MaxColorAttachments + 1);
        rejects([&] { desc.validate(); });
        desc.colors = {{ColorFormat::RGBA16F}};
        desc.samples = 4;
        rejects([&] { desc.validate(); }); // unsupported by this wrapper
        desc.colors = {{ColorFormat::RGB8}};
        desc.validate();
        desc.samples = 0;
        rejects([&] { desc.validate(); });
        desc.samples = 1;
        desc.depth = {DepthStorage::Cubemap, DepthFormat::Depth};
        rejects([&] { desc.validate(); }); // layered + non-layered
        desc.colors.clear();
        desc.validate();
        desc.extent.height = 32;
        rejects([&] { desc.validate(); });
        desc.extent = {64, 64};
        desc.depth = {DepthStorage::Texture2D, DepthFormat::Depth24Stencil8};
        rejects([&] { desc.validate(); });
        desc.depth = {DepthStorage::Renderbuffer, DepthFormat::Depth24};
        rejects([&] { desc.validate(); });
        desc.depth = DepthAttachmentDesc{};
        desc.validate();
        desc.extent.width = (std::numeric_limits<unsigned int>::max)();
        rejects([&] { desc.validate(); });
        // Invalid construction must throw before touching the uninitialized GL loader.
        rejects([&] { FrameBuffer framebuffer(desc); });
    }

    void checkComplete(const FrameBuffer& buffer)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, buffer.getFBO());
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "incomplete FBO");
    }

    void checkTexture(GLuint texture, GLenum format, GLenum filter, unsigned int width)
    {
        require(texture != 0, "missing texture");
        glBindTexture(GL_TEXTURE_2D, texture);
        GLint actualFormat = 0, actualFilter = 0, actualWidth = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &actualFormat);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &actualWidth);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &actualFilter);
        require(actualFormat == static_cast<GLint>(format), "wrong texture format");
        require(actualFilter == static_cast<GLint>(filter), "wrong texture filter");
        require(actualWidth == static_cast<GLint>(width), "wrong texture width");
    }

    void gpuTests()
    {
        RenderTargets targets;
        targets.initialize({64, 48}, 32, ColorFormat::RGBA16F);
        targets.syncPointShadows(4, 32);
        for (const auto* target : {targets.hdr.get(), targets.gbuffer.get(), targets.deferredLighting.get(),
            targets.finalOutput.get(), targets.directionalShadow.get(), targets.bloomPingPong[0].get(), targets.bloomPingPong[1].get()})
            checkComplete(*target);
        for (const auto& target : targets.pointShadows)
        {
            checkComplete(*target);
            require(target->getDepthCube() != 0 && target->getColor() == 0, "point shadow attachments wrong");
        }
        for (int i = 0; i < 5; ++i) checkTexture(targets.gbuffer->getColor(i), GL_RGBA16F, GL_NEAREST, 64);
        for (const auto* target : {targets.hdr.get(), targets.deferredLighting.get()})
            for (int i = 0; i < 2; ++i) checkTexture(target->getColor(i), GL_RGBA16F, GL_LINEAR, 64);
        checkTexture(targets.gbuffer->getDepth2D(), GL_DEPTH_COMPONENT24, GL_NEAREST, 64);
        checkTexture(targets.finalOutput->getColor(), GL_RGB8, GL_LINEAR, 64);
        for (const auto& target : targets.bloomPingPong)
        {
            checkTexture(target->getColor(), GL_RGBA16F, GL_LINEAR, 64);
            checkTexture(target->getDepth2D(), GL_DEPTH_COMPONENT24, GL_NEAREST, 64);
        }
        glBindTexture(GL_TEXTURE_2D, targets.directionalShadow->getDepth2D());
        GLint wrap = 0;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &wrap);
        require(wrap == GL_CLAMP_TO_BORDER, "directional shadow border changed");

        // MRT writes and depth blit are both used by the actual pipeline.
        checkComplete(*targets.gbuffer);
        const float color[] = {0.25f, 0.5f, 0.75f, 1.0f};
        for (int i = 0; i < 5; ++i)
        {
            glClearBufferfv(GL_COLOR, i, color);
            glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
            float pixel[4] = {};
            glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, pixel);
            require(std::fabs(pixel[1] - color[1]) < 0.01f, "MRT attachment readback failed");
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, targets.deferredLighting->getFBO());
        glBlitFramebuffer(0, 0, 64, 48, 0, 0, 64, 48, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        require(glGetError() == GL_NO_ERROR, "GL error in creation/readback/depth blit");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        const GLuint oldOutput = targets.finalOutput->getColor();
        const GLuint shadow = targets.directionalShadow->getFBO();
        const GLuint point = targets.pointShadows[0]->getFBO();
        targets.resizeViewport({80, 60});
        require(!glIsTexture(oldOutput), "old output leaked after resize");
        require(targets.directionalShadow->getFBO() == shadow && targets.pointShadows[0]->getFBO() == point,
            "viewport resize changed shadow allocations");
        for (const auto* target : {targets.hdr.get(), targets.gbuffer.get(), targets.deferredLighting.get(),
            targets.finalOutput.get(), targets.bloomPingPong[0].get(), targets.bloomPingPong[1].get()})
        {
            checkComplete(*target);
            require(target->getWidth() == 80 && target->getHeight() == 60, "viewport size mismatch");
        }
        const GLuint stable = targets.hdr->getFBO();
        targets.resizeViewport({80, 60});
        require(stable == targets.hdr->getFBO(), "same-size resize reallocated targets");
        rejects([&] { targets.resizeViewport({0, 60}); });
        require(stable == targets.hdr->getFBO(), "failed resize damaged old targets");

        FramebufferDesc colorOnly;
        colorOnly.extent = {16, 16};
        colorOnly.colors = {{ColorFormat::RGB8}};
        colorOnly.debugName = "Color only / move test";
        FrameBuffer source(colorOnly);
        checkComplete(source);
        GLint depthType = -1;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &depthType);
        require(depthType == GL_NONE, "color-only buffer unexpectedly allocated depth");
        const GLuint movedId = source.getFBO();
        const GLuint movedTexture = source.getColor();
        {
            FrameBuffer moved(std::move(source));
            require(source.getFBO() == 0 && moved.getFBO() == movedId, "move construction lost ownership");
            FrameBuffer destination(colorOnly);
            const GLuint replacedId = destination.getFBO();
            destination = std::move(moved);
            require(moved.getFBO() == 0 && destination.getFBO() == movedId && !glIsFramebuffer(replacedId),
                "move assignment leaked or duplicated ownership");
            rejects([&] { destination.resize(0, 16); });
            require(destination.getFBO() == movedId, "failed individual resize lost allocation");
            destination.resize(20, 20);
            checkComplete(destination);
        }
        require(!glIsFramebuffer(movedId) && !glIsTexture(movedTexture), "move/resize leaked GL objects");

        colorOnly.samples = 4;
        colorOnly.depth = DepthAttachmentDesc{};
        FrameBuffer multisample(colorOnly);
        checkComplete(multisample);
        const GLuint droppedShadow = targets.pointShadows.back()->getFBO();
        targets.syncPointShadows(1, 32);
        require(!glIsFramebuffer(droppedShadow), "shrinking point lights leaked shadow FBO");
        targets = {};
        require(!glIsFramebuffer(shadow) && !glIsFramebuffer(point), "target cleanup leaked shadows");
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        require(glGetError() == GL_NO_ERROR, "GL error in resize/move/cleanup");
    }
}

int main(int argc, char** argv)
{
    GLFWwindow* window = nullptr;
    const bool gpu = argc > 1 && std::string(argv[1]) == "--gpu";
    int result = 0;
    try
    {
        descriptorTests();
        if (gpu)
        {
            require(glfwInit() == GLFW_TRUE, "GLFW initialization failed");
            glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
            glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
            glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
            window = glfwCreateWindow(64, 64, "Framebuffer tests", nullptr, nullptr);
            require(window != nullptr, "hidden GL context creation failed");
            glfwMakeContextCurrent(window);
            require(gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0, "GL loader failed");
            std::cout << "OpenGL: " << glGetString(GL_VERSION) << '\n';
            gpuTests(); // All GL resources destruct before the context.
        }
        std::cout << (gpu ? "Framebuffer GPU tests passed\n" : "Framebuffer descriptor tests passed\n");
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; result = 1; }
    if (window) glfwDestroyWindow(window);
    if (gpu) glfwTerminate();
    return result;
}
