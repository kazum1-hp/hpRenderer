#pragma once
#include <glad/glad.h>
#include <string>

enum TextureType
{
	Diffuse,
	Specular,
	Normal,
	Height,
	ARM,
	HDR,
	AttachMent
};


class Texture
{
private:
	unsigned int ID;
	std::string path;
	TextureType type;

public:
	Texture(const std::string& path, TextureType typeName = Diffuse);

	Texture(GLuint textureID, TextureType typeName = AttachMent)
		: ID(textureID), type(typeName) {
	}

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	Texture(Texture&& other) noexcept
		:ID(other.ID), type(other.type)
	{
		other.ID = 0;
	}

	Texture& operator=(Texture&& other) noexcept
	{
		if (this != &other)
		{
			glDeleteTextures(1, &ID);
			ID = other.ID;
			other.ID = 0;
		}
		return *this;
	}

	void bind(GLuint slot) const;
	GLuint getID() const { return ID; }
	std::string getPath() const { return path; };
	TextureType getType() const { return type; }
	~Texture();
};