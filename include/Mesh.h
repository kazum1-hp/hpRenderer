#pragma once

#include "Geometry.h"
#include "Texture.h"
#include "Shader.h"

#include <memory>

class Mesh
{
public:
	Mesh(const Geometry& geometry, const std::vector<std::shared_ptr<Texture>>& texs = {});
	~Mesh();

	Mesh(const Mesh&) = delete;
	Mesh& operator=(const Mesh&) = delete;
	Mesh(Mesh&& other) noexcept;
	Mesh& operator=(Mesh&& other) noexcept;

	void draw() const;
	void drawInstanced(int instanceCount) const;

	void setInstanceTransforms(const std::vector<glm::mat4>& transforms);
	
	GLsizei getIndexCount() const { return indexCount; }

	GLuint getVAO() const { return VAO; }
	void addTexture(const std::shared_ptr<Texture>& texture) {
		textures.push_back(texture);
	}
	const std::vector<std::shared_ptr<Texture>>& getTexture() const { return textures; }

private:
	GLuint VAO = 0, VBO = 0, EBO = 0;
	GLuint instanceVBO = 0;

	GLsizei indexCount = 0;
	std::vector<VertexAttribute> attributes;
	std::vector<std::shared_ptr<Texture>> textures;

	void cleanUp();
	void moveFrom(Mesh&& other) noexcept;

	//bool hasNormalMap;
};

