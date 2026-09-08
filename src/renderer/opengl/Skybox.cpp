#include "hpr/renderer/opengl/Skybox.h"
#include <iostream>
#include <algorithm>
Skybox::~Skybox()
{
    if (id)
        glDeleteTextures(1, &id);
}
bool Skybox::LoadFromFiles(const std::vector<std::string>& paths)
{
    if (paths.size() != 6)
        return false;
    std::array<std::string, 6> faces;
    std::copy(paths.begin(), paths.end(), faces.begin());
    std::string error;
    auto asset = SkyboxAsset::Load(faces, error);
    if (!asset)
    {
        std::cerr << error << '\n';
        return false;
    }
    return load(*asset);
}
bool Skybox::load(const SkyboxAsset& asset)
{
    for (const auto& image : asset.faces)
        if (!image.valid() || image.isHDR() || image.width != image.height || image.width != asset.faces[0].width)
            return false;
    GLint binding, alignment, rowLength, skipRows, skipPixels, buffer;
    glGetIntegerv(GL_TEXTURE_BINDING_CUBE_MAP, &binding);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &buffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    GLuint replacement = 0;
    glGenTextures(1, &replacement);
    glBindTexture(GL_TEXTURE_CUBE_MAP, replacement);
    bool valid = true;
    for (size_t i = 0; i < 6; ++i)
    {
        const auto& image = asset.faces[i];
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(i), 0, GL_SRGB8_ALPHA8, image.width,
                     image.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image.rgba.data());
        GLint width = 0;
        glGetTexLevelParameteriv(GL_TEXTURE_CUBE_MAP_POSITIVE_X + static_cast<GLenum>(i), 0, GL_TEXTURE_WIDTH, &width);
        valid &= width == image.width;
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, binding);
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, buffer);
    if (!valid)
    {
        glDeleteTextures(1, &replacement);
        return false;
    }
    if (id)
        glDeleteTextures(1, &id);
    id = replacement;
    return true;
}
