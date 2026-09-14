#include "hpr/renderer/opengl/Texture.h"
#include "hpr/assets/ImageData.h"
#include <iostream>
#include <climits>
Texture::Texture(const std::string& path, TextureType semantic, ColorSpace space)
    : Texture(ImageData::Load(path, semantic == HDR), semantic, space)
{
    this->path = path;
}
Texture::Texture(const ImageData& image, TextureType semantic, ColorSpace space) : type(semantic), colorSpace(space)
{
    if (!image.valid() || semantic == AttachMent || (image.isHDR() != (semantic == HDR)) ||
        (image.isHDR() && space != ColorSpace::Linear))
        return;
    upload(image.isHDR() ? static_cast<const void*>(image.hdr.data()) : image.rgba.data(), image.width, image.height,
           4);
}
Texture::Texture(const std::string& label, const std::vector<unsigned char>& bytes, unsigned int width,
                 unsigned int height, TextureType semantic, ColorSpace space)
    : path(label), type(semantic), colorSpace(space)
{
    if (semantic == AttachMent || semantic == HDR)
        return;
    if (width == 0 && height == 0)
    {
        const auto image = ImageData::Decode(bytes);
        if (image.valid())
            upload(image.rgba.data(), image.width, image.height, 4);
    }
    else if (width > 0 && height > 0 && width <= INT_MAX && height <= INT_MAX &&
             static_cast<size_t>(width) * height <= bytes.size() / 4)
        upload(bytes.data(), static_cast<int>(width), static_cast<int>(height), 4);
}
void Texture::upload(const void* pixels, int width, int height, int channels)
{
    const auto typeName = type;
    const auto space = colorSpace;
    if (type != HDR && (channels == 2 || channels == 4 || type == Opacity))
    {
        const auto* bytes = static_cast<const unsigned char*>(pixels);
        const int alphaChannel = type == Opacity ? 0 : channels - 1;
        const std::size_t count = static_cast<std::size_t>(width) * height;
        std::size_t visible = 0, intermediate = 0;
        for (std::size_t i = 0; i < count; ++i)
        {
            const unsigned char alpha = bytes[i * channels + alphaChannel];
            transparent |= alpha < 255;
            visible += alpha > 8;
            intermediate += alpha > 8 && alpha < 247;
        }
        // Formats without alphaMode need a heuristic: binary alpha and narrow
        // antialiased edges are cutouts. A substantial intermediate-alpha region
        // is blended. Count visible texels so atlas padding cannot dilute it.
        translucent = intermediate > visible / 20;
    }

    const GLenum formats[] = {GL_RED, GL_RG, GL_RGB, GL_RGBA};
    const GLenum linearFormats[] = {GL_R8, GL_RG8, GL_RGB8, GL_RGBA8};
    const GLenum floatFormats[] = {GL_R16F, GL_RG16F, GL_RGB16F, GL_RGBA16F};
    const GLenum internal = typeName == HDR             ? floatFormats[channels - 1]
                            : space == ColorSpace::SRGB ? GL_SRGB8_ALPHA8
                                                        : linearFormats[channels - 1];

    // Upload tightly packed rows independently of prior editor/renderer state.
    GLint active, binding, alignment, rowLength, skipRows, skipPixels, unpackBuffer;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    glGetIntegerv(GL_UNPACK_ALIGNMENT, &alignment);
    glGetIntegerv(GL_UNPACK_ROW_LENGTH, &rowLength);
    glGetIntegerv(GL_UNPACK_SKIP_ROWS, &skipRows);
    glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &skipPixels);
    glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    glGenTextures(1, &ID);
    glBindTexture(GL_TEXTURE_2D, ID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, typeName == HDR ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, typeName == HDR ? GL_CLAMP_TO_EDGE : GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, typeName == HDR ? GL_LINEAR : GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, internal, width, height, 0, formats[channels - 1],
                 typeName == HDR ? GL_FLOAT : GL_UNSIGNED_BYTE, pixels);
    GLint uploadedWidth = 0;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &uploadedWidth);
    valid = uploadedWidth == width;
    if (valid)
        glGenerateMipmap(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, binding);
    glActiveTexture(active);
    glPixelStorei(GL_UNPACK_ALIGNMENT, alignment);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, skipRows);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, skipPixels);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
}
void Texture::bind(GLuint slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, ID);
}
Texture::~Texture()
{
    if (ID)
        glDeleteTextures(1, &ID);
}
