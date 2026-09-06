#include "FrameBuffer.h"
#include <iostream>
#include <utility>

FrameBuffer::FrameBuffer(FramebufferDesc description) : desc(std::move(description))
{
    desc.validate();
    try { init(); }
    catch (...) { cleanUp(); throw; }
}

void FrameBuffer::init()
{
    GLint maxColors = 0, maxDrawBuffers = 0, maxTextureSize = 0, maxRenderbufferSize = 0, maxSamples = 0;
    glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColors);
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    glGetIntegerv(desc.depth && desc.depth->storage == DepthStorage::Cubemap ?
        GL_MAX_CUBE_MAP_TEXTURE_SIZE : GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbufferSize);
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    const auto exceeds = [this](GLint limit) {
        return limit <= 0 || desc.extent.width > static_cast<unsigned int>(limit) ||
            desc.extent.height > static_cast<unsigned int>(limit);
    };
    if (static_cast<GLint>(desc.colors.size()) > maxColors || static_cast<GLint>(desc.colors.size()) > maxDrawBuffers ||
        exceeds(maxTextureSize) ||
        (desc.depth && desc.depth->storage == DepthStorage::Renderbuffer && exceeds(maxRenderbufferSize)) ||
        (desc.samples > 1 && desc.samples > static_cast<unsigned int>(maxSamples)))
        throw std::runtime_error("Framebuffer '" + desc.debugName + "' exceeds current OpenGL limits");

    const auto w = static_cast<GLsizei>(desc.extent.width);
    const auto h = static_cast<GLsizei>(desc.extent.height);
    glGenFramebuffers(1, &FBO);
    glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    std::array<GLenum, FramebufferDesc::MaxColorAttachments> attachments{};
    for (std::size_t i = 0; i < desc.colors.size(); ++i)
    {
        const auto& color = desc.colors[i];
        const bool hdr = color.format == ColorFormat::RGBA16F;
        const GLenum target = desc.samples > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        glGenTextures(1, &texColors[i]);
        glBindTexture(target, texColors[i]);
        if (desc.samples > 1)
            glTexImage2DMultisample(target, desc.samples, GL_RGB8, w, h, GL_TRUE);
        else
        {
            glTexImage2D(target, 0, hdr ? GL_RGBA16F : GL_RGB8, w, h, 0,
                hdr ? GL_RGBA : GL_RGB, hdr ? GL_FLOAT : GL_UNSIGNED_BYTE, nullptr);
            const GLenum filter = color.filter == TextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filter);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filter);
            glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
        attachments[i] = GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(i);
        glFramebufferTexture2D(GL_FRAMEBUFFER, attachments[i], target, texColors[i], 0);
    }
    if (desc.colors.empty())
    {
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    else glDrawBuffers(static_cast<GLsizei>(desc.colors.size()), attachments.data());

    if (desc.depth)
    {
        const auto& depth = *desc.depth;
        if (depth.storage == DepthStorage::Renderbuffer)
        {
            glGenRenderbuffers(1, &RBO);
            glBindRenderbuffer(GL_RENDERBUFFER, RBO);
            if (desc.samples > 1)
                glRenderbufferStorageMultisample(GL_RENDERBUFFER, desc.samples, GL_DEPTH24_STENCIL8, w, h);
            else glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
        }
        else
        {
            const bool cube = depth.storage == DepthStorage::Cubemap;
            GLuint& texture = cube ? texDepthCube : texDepth2D;
            const GLenum target = cube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
            const GLenum format = depth.format == DepthFormat::Depth24 ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT;
            glGenTextures(1, &texture);
            glBindTexture(target, texture);
            for (int face = 0; face < (cube ? 6 : 1); ++face)
                glTexImage2D(cube ? GL_TEXTURE_CUBE_MAP_POSITIVE_X + face : target, 0, format,
                    w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
            glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            const GLenum wrap = depth.wrap == DepthWrap::ClampToBorder ? GL_CLAMP_TO_BORDER : GL_CLAMP_TO_EDGE;
            glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap);
            glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap);
            if (cube) glTexParameteri(target, GL_TEXTURE_WRAP_R, wrap);
            if (depth.wrap == DepthWrap::ClampToBorder)
            {
                const float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
                glTexParameterfv(target, GL_TEXTURE_BORDER_COLOR, borderColor);
            }
            if (cube) glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture, 0);
            else glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, target, texture, 0);
        }
    }
    const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("Framebuffer '" + desc.debugName + "' is incomplete (status " + std::to_string(status) + ")");
    std::cout << "FBO: " << FBO << " ('" << desc.debugName << "'), FrameBuffer is complete!" << std::endl;
}

void FrameBuffer::cleanUp()
{
    for (auto& color : texColors)
    {
        if (color) glDeleteTextures(1, &color);
        color = 0;
    }
    if (texDepth2D) glDeleteTextures(1, &texDepth2D);
    if (texDepthCube) glDeleteTextures(1, &texDepthCube);
    if (RBO) glDeleteRenderbuffers(1, &RBO);
    if (FBO) glDeleteFramebuffers(1, &FBO);
    texDepth2D = texDepthCube = RBO = FBO = 0;
}

void FrameBuffer::resize(unsigned int newWidth, unsigned int newHeight)
{
    if (newWidth == getWidth() && newHeight == getHeight()) return;
    auto resized = desc;
    resized.extent = { newWidth, newHeight };
    // Keep the old allocation alive if validation or creation fails.
    FrameBuffer replacement(std::move(resized));
    *this = std::move(replacement);
}

FrameBuffer::~FrameBuffer() { cleanUp(); }
FrameBuffer::FrameBuffer(FrameBuffer&& other) noexcept { moveFrom(std::move(other)); }
FrameBuffer& FrameBuffer::operator=(FrameBuffer&& other) noexcept
{
    if (this != &other)
    {
        cleanUp();
        moveFrom(std::move(other));
    }
    return *this;
}

void FrameBuffer::moveFrom(FrameBuffer&& other) noexcept
{
    desc = std::move(other.desc);
    FBO = std::exchange(other.FBO, 0);
    RBO = std::exchange(other.RBO, 0);
    for (std::size_t i = 0; i < texColors.size(); ++i)
        texColors[i] = std::exchange(other.texColors[i], 0);
    texDepth2D = std::exchange(other.texDepth2D, 0);
    texDepthCube = std::exchange(other.texDepthCube, 0);
    other.desc.extent = {};
}
