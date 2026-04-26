#pragma once

#include "Window.h"
#include "Texture.h"
#include "Mesh.h"

class FrameBuffer
{
private:
	unsigned int width, height;
	GLuint FBO = 0, RBO = 0;
	static constexpr int MAX_COLOR_ATTACHMENTS = 8;
	GLuint texColors[MAX_COLOR_ATTACHMENTS] = {0};
	GLuint texDepth2D, texDepthCube;
	GLuint gDepth;
	bool m_useDepth;
	bool m_useMs;
	bool m_useDepthMap2D;
	bool m_useDepthCube;
	bool m_useHdr;
	bool m_useGbuffer;
	int m_colorCount;

	void init(unsigned int w, unsigned int h);
	void cleanUp();

public:
	FrameBuffer(Window& window, bool useDepth = true, bool useMs = false, bool useDepthMap2D = false, bool useDepthCube = false, bool useHdr = false, int colorCount = 1, bool useGbuffer = false);

	FrameBuffer(unsigned int w = 1920, unsigned int h = 1080, bool useDepth = true, bool useMs = false, bool useDepthMap2D = false, bool useDepthCube = false, bool useHdr = false, int colorCount = 1, bool useGbuffer = false);

	~FrameBuffer();

	void resize(unsigned int newWidth, unsigned int newHeight);

	GLuint getFBO() const { return FBO; }
	GLuint getColor(int index = 0) const 
	{
		if (index < 0 || index >= m_colorCount)
			return 0;

		return texColors[index]; 
	}
	GLuint getGDepth() const { return gDepth; }
	GLuint getDepth2D() const { return texDepth2D; }
	GLuint getDepthCube() const { return texDepthCube; }
	unsigned int getWidth() const { return width; }
	unsigned int getHeight() const { return height; }
};

