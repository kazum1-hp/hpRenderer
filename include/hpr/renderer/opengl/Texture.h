#pragma once
#include <glad/glad.h>
#include <string>
#include <utility>
#include <vector>

#include "hpr/assets/TextureTypes.h"
struct ImageData;

class Texture
{
private:
    unsigned int ID = 0;
    std::string path;
    TextureType type;
    ColorSpace colorSpace = ColorSpace::Linear;
    bool valid = false;
    bool transparent = false;
    bool translucent = false;
    void upload(const void* pixels, int width, int height, int channels);

public:
    Texture(const std::string& path, TextureType typeName = Diffuse)
        : Texture(path, typeName, DefaultColorSpace(typeName))
    {
    }
    Texture(const std::string& path, TextureType typeName, ColorSpace colorSpace);
    Texture(const ImageData& image, TextureType semantic, ColorSpace space);
    // width/height == 0 decodes an encoded image; otherwise bytes are RGBA8.
    Texture(const std::string& label, const std::vector<unsigned char>& bytes,
            unsigned int width, unsigned int height, TextureType semantic, ColorSpace space);

    Texture(GLuint textureID, TextureType typeName = AttachMent) : ID(textureID), type(typeName), valid(textureID != 0)
    {
    }

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(Texture&& other) noexcept
        : ID(other.ID), path(std::move(other.path)), type(other.type), colorSpace(other.colorSpace), valid(other.valid),
          transparent(other.transparent), translucent(other.translucent)
    {
        other.ID = 0;
        other.valid = false;
    }

    Texture& operator=(Texture&& other) noexcept
    {
        if (this != &other)
        {
            glDeleteTextures(1, &ID);
            ID = other.ID;
            path = std::move(other.path);
            type = other.type;
            colorSpace = other.colorSpace;
            valid = other.valid;
            transparent = other.transparent;
            translucent = other.translucent;
            other.ID = 0;
            other.valid = false;
        }
        return *this;
    }

    void bind(GLuint slot) const;
    GLuint getID() const
    {
        return ID;
    }
    std::string getPath() const
    {
        return path;
    };
    TextureType getType() const
    {
        return type;
    }
    ColorSpace getColorSpace() const
    {
        return colorSpace;
    }
    bool isValid() const
    {
        return valid && ID != 0;
    }
    bool hasTransparency() const { return transparent; }
    // Legacy alpha inference: ignore a small fringe of intermediate alpha on cutouts.
    bool hasTranslucency() const { return translucent; }
    ~Texture();
};
