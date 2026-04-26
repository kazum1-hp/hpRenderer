#include "../include/FrameBuffer.h"
#include <iostream>

FrameBuffer::FrameBuffer(Window& window, bool useDepth, bool useMs, bool useDepthMap2D, bool useDepthCube, bool useHdr, int colorCount, bool useGbuffer)
	: width(window.getWidth()), height(window.getHeight()),
      m_useDepth(useDepth), m_useMs(useMs), m_useDepthMap2D(useDepthMap2D), m_useDepthCube(useDepthCube), m_useHdr(useHdr), m_colorCount(colorCount), m_useGbuffer(useGbuffer)
{
	init(width, height);
}

FrameBuffer::FrameBuffer(unsigned int w, unsigned int h, bool useDepth, bool useMs, bool useDepthMap2D, bool useDepthCube, bool useHdr, int colorCount, bool useGbuffer)
	: width(w), height(h),
      m_useDepth(useDepth), m_useMs(useMs), m_useDepthMap2D(useDepthMap2D), m_useDepthCube(useDepthCube), m_useHdr(useHdr), m_colorCount(colorCount), m_useGbuffer(useGbuffer)
{
	init(width, height);
}

void FrameBuffer::init(unsigned int w, unsigned int h)
{
	glGenFramebuffers(1, &FBO);
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

    if (m_colorCount > 0 && !m_useDepthMap2D && !m_useDepthCube)
    {
        if (!m_useMs) {
            for (int i = 0; i < m_colorCount; ++i)
            {
                // color attachment (non-MSAA)
                glGenTextures(1, &texColors[i]);
                glBindTexture(GL_TEXTURE_2D, texColors[i]);

                // useHdr or not
                GLenum internalFormat = m_useHdr ? GL_RGBA16F : GL_RGB;
                GLenum format = m_useHdr ? GL_RGBA : GL_RGB;
                GLenum type = m_useHdr ? GL_FLOAT : GL_UNSIGNED_BYTE;
                GLenum pname = m_useGbuffer ? GL_NEAREST : GL_LINEAR;
                glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, pname);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, pname);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + i, GL_TEXTURE_2D, texColors[i], 0);
            }

            if (!m_useDepth)
            {
                glGenTextures(1, &gDepth);
                glBindTexture(GL_TEXTURE_2D, gDepth);

                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, gDepth, 0);
            }
        }
        else {
            // MSAA texture£¨do not support HDR£¬only 1 color attachment£©
            if (m_colorCount > 1) {
                std::cerr << "ERROR: MSAA Framebuffer cannot have multiple color attachments!" << std::endl;
                m_colorCount = 1;  // reduce to 1
            }
            if (m_useHdr) {
                std::cerr << "ERROR: MSAA does not support HDR format! Forcing RGB8" << std::endl;
            }

            // multisample color texture attachment
            glGenTextures(1, &texColors[0]);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, texColors[0]);
            glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, 4, GL_RGB, w, h, GL_TRUE);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, 0);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, texColors[0], 0);
        }
        std::vector<GLenum> attachments;
        for (int i = 0; i < m_colorCount; ++i)
            attachments.push_back(GL_COLOR_ATTACHMENT0 + i);
        glDrawBuffers(static_cast<GLsizei>(attachments.size()), attachments.data());
    }
    else {
        // ShadowMap£ºno color attachment
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    // depth buffer options:
    // 1) useDepthMap2D => create and attach a 2D depth texture (for directional shadow maps)
    // 2) useDepthCube  => create and attach a cube depth texture (for point light shadow maps)
    // 3) useDepth (renderbuffer) => create RBO depth/stencil

    if (m_useDepthMap2D) {
        // create 2D depth texture
        glGenTextures(1, &texDepth2D);
        glBindTexture(GL_TEXTURE_2D, texDepth2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = { 1.0f,1.0f,1.0f,1.0f };
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texDepth2D, 0);
    }
    else if (m_useDepthCube) {
        // create cube depth texture
        glGenTextures(1, &texDepthCube);
        glBindTexture(GL_TEXTURE_CUBE_MAP, texDepthCube);
        for (unsigned int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_DEPTH_COMPONENT,
                w, h, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        // attach the whole cubemap as depth attachment (valid: will allow layered rendering with GS)
        glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texDepthCube, 0);
    }
    else if (m_useDepth) {
        // renderbuffer depth/stencil
        glGenRenderbuffers(1, &RBO);
        glBindRenderbuffer(GL_RENDERBUFFER, RBO);
        if (m_useMs) {
            // multisample RBO handled elsewhere in code path; but keep here for completeness
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, 4, GL_DEPTH24_STENCIL8, w, h);
        }
        else {
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
        }
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, RBO);
    }

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "ERROR :: FRAMEBUFFER :: FrameBuffer is not complete!" << std::endl;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void FrameBuffer::cleanUp()
{
    // Clean up existing resources (similar to destructor)
    for (int i = 0; i < m_colorCount; ++i)
    {
        if (texColors[i]) {
            glDeleteTextures(1, &texColors[i]);
            texColors[i] = 0;
        }
    }
    if (texDepth2D) {
        glDeleteTextures(1, &texDepth2D);
        texDepth2D = 0;
    }
    if (texDepthCube) {
        glDeleteTextures(1, &texDepthCube);
        texDepthCube = 0;
    }
    if (RBO) {
        glDeleteRenderbuffers(1, &RBO);
        RBO = 0;
    }
    if (FBO) {
        glDeleteFramebuffers(1, &FBO);
        FBO = 0;
    }
}

void FrameBuffer::resize(unsigned int newWidth, unsigned int newHeight)
{
    if (newWidth == width && newHeight == height) { return; }  // No change needed

    cleanUp();

    // Update dimensions
    width = newWidth;
    height = newHeight;

    // Re-initialize with new size
    init(width, height);
}

FrameBuffer::~FrameBuffer()
{
    cleanUp();
}