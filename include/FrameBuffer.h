#pragma once

#include <glad/glad.h>
#include <array>
#include "FramebufferDesc.h"

class FrameBuffer
{
public:
    explicit FrameBuffer(FramebufferDesc desc);
    ~FrameBuffer();

    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;
    FrameBuffer(FrameBuffer&& other) noexcept;
    FrameBuffer& operator=(FrameBuffer&& other) noexcept;

    void resize(unsigned int newWidth, unsigned int newHeight);
    const FramebufferDesc& getDesc() const { return desc; }
    GLuint getFBO() const { return FBO; }
    GLuint getColor(int index = 0) const
    {
        return index >= 0 && static_cast<std::size_t>(index) < desc.colors.size() ? texColors[index] : 0;
    }
    GLuint getDepth2D() const { return texDepth2D; }
    GLuint getDepthCube() const { return texDepthCube; }
    unsigned int getWidth() const { return desc.extent.width; }
    unsigned int getHeight() const { return desc.extent.height; }

private:
    FramebufferDesc desc;
    GLuint FBO = 0, RBO = 0;
    std::array<GLuint, FramebufferDesc::MaxColorAttachments> texColors{};
    GLuint texDepth2D = 0, texDepthCube = 0;

    void init();
    void cleanUp();
    void moveFrom(FrameBuffer&& other) noexcept;
};

