#pragma once
#include <glad/glad.h>
#include <string>
#include <utility>

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
	unsigned int ID = 0;
	std::string path;
	TextureType type;
	bool valid = false;

public:
	Texture(const std::string& path, TextureType typeName = Diffuse);

	Texture(GLuint textureID, TextureType typeName = AttachMent)
		: ID(textureID), type(typeName), valid(textureID != 0) {
	}

	Texture(const Texture&) = delete;
	Texture& operator=(const Texture&) = delete;

	Texture(Texture&& other) noexcept
		:ID(other.ID), path(std::move(other.path)), type(other.type), valid(other.valid)
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
			valid = other.valid;
			other.ID = 0;
			other.valid = false;
		}
		return *this;
	}

	void bind(GLuint slot) const;
	GLuint getID() const { return ID; }
	std::string getPath() const { return path; };
	TextureType getType() const { return type; }
	bool isValid() const { return valid && ID != 0; }
	~Texture();
};
