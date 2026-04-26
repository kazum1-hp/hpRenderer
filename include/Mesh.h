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

	void draw() const;
	void drawInstanced(int instanceCount) const;

	void setInstanceTransforms(const std::vector<glm::mat4>& transforms);
	
	GLsizei getIndexCount() const { return indexCount; }

	GLuint getVAO() const { return VAO; }
	void addTexture(const std::shared_ptr<Texture>& texture) {
		textures.push_back(texture);
	}
	std::vector<std::shared_ptr<Texture>> getTexture() const { return textures; }

private:
	GLuint VAO, VBO, EBO;
	GLuint instanceVBO = 0;

	GLsizei indexCount;
	std::vector<VertexAttribute> attributes;
	std::vector<std::shared_ptr<Texture>> textures;

	//bool hasNormalMap;
};

