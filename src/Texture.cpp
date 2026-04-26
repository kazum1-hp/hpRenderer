#define STBI_NO_STDIO   // forbide stb_image use fopen
#include "../third_party/stb/stb_image.h"

#include "../include/Texture.h"
#include <iostream>
#ifdef APIENTRY
#undef APIENTRY
#endif
#include <windows.h> // for MultiByteToWideChar
#include <fstream>
#include <vector>
#include <filesystem>

Texture::Texture(const std::string& path, TextureType typeName)
	: path(path), type(typeName)
{
	glGenTextures(1, &ID);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, ID);

	if (typeName == HDR)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	
	// Flip upside down
	//stbi_set_flip_vertically_on_load(true);

	// UTF-8 -> UTF-16
	int wlen = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
	std::wstring wpath(wlen, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, &wpath[0], wlen);

	// Debug output: Prints the path (wide characters) that will be attempted to be opened.
	std::wcout.imbue(std::locale(""));
	std::wcout << L"[Texture] Trying to open (wpath): " << wpath << std::endl;
	std::cout << "[Texture] trying to open (utf8): " << path << std::endl;

	// Use the filesystem tool to check if a file exists (to aid in diagnosis).
	try {
		std::filesystem::path fp = std::filesystem::path(wpath);
		if (!std::filesystem::exists(fp)) {
			std::cerr << "Failed to open texture file (not found): " << path << std::endl;
			return;
		}
	} catch (const std::exception& e) {
		std::cerr << "Filesystem check exception: " << e.what() << " for path: " << path << std::endl;
		// Continue trying to open with ifstream
	}

	// read file to memory by using ifstream
	std::ifstream file(wpath, std::ios::binary | std::ios::ate);
	if (!file) {
		std::cerr << "Failed to open texture file: " << path << std::endl;
		return;
	}

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);
	std::vector<unsigned char> buffer(size);
	if (!file.read((char*)buffer.data(), size)) {
		std::cerr << "Failed to read texture file: " << path << std::endl;
		return;
	}

	GLenum internalformat, format, type;
	// Number of Width, height and Color channels
	int  width, height, nrChannels;
	void* data = nullptr;

	// load from memory
	if (typeName == HDR) {
		stbi_set_flip_vertically_on_load(true);
		data = stbi_loadf_from_memory(buffer.data(), (int)size, &width, &height, &nrChannels, 0);
	}
	else {
		data = stbi_load_from_memory(buffer.data(), (int)size, &width, &height, &nrChannels, 0);
	}

	/*unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);*/

	if (data)
	{
		if (nrChannels == 3)
		{
			if (typeName == Diffuse) { internalformat = GL_SRGB; type = GL_UNSIGNED_BYTE; }
			else if (typeName == HDR) { internalformat = GL_RGB16F; type = GL_FLOAT; }
			else { internalformat = GL_RGB; type = GL_UNSIGNED_BYTE; }
			format = GL_RGB;

			glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, type, data);
		}

		else if (nrChannels == 4)
		{
			if (typeName == Diffuse) { internalformat = GL_SRGB_ALPHA; type = GL_UNSIGNED_BYTE; }
			else if (typeName == HDR) { internalformat = GL_RGBA16F; type = GL_FLOAT; }
			else { internalformat = GL_RGBA; type = GL_UNSIGNED_BYTE; }
			format = GL_RGBA;

			glTexImage2D(GL_TEXTURE_2D, 0, internalformat, width, height, 0, format, type, data);
		}

		glGenerateMipmap(GL_TEXTURE_2D);

		std::cout << "Loading texture: " << path << " ID: " << ID << " texType: " << typeName << std::endl;
		//std::cout << "Size: " << width << "x" << height << ", Channels: " << nrChannels << std::endl;
	}

	else
	{
		std::cerr << "Failed to load texture" << std::endl;
	}

	stbi_image_free(data);
}

void Texture::bind(GLuint slot) const
{
	glActiveTexture(GL_TEXTURE0 + slot);
	glBindTexture(GL_TEXTURE_2D, ID);
}

Texture::~Texture()
{
	glDeleteTextures(1, &ID);
}