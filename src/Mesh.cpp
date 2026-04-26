#include "../include/Mesh.h"
#include <iostream>


Mesh::Mesh(const Geometry& geometry, const std::vector<std::shared_ptr<Texture>>& texs)
	:textures(texs)
{
	const auto& vertices = geometry.vertices;
	const auto& indices = geometry.indices;
	this->attributes = geometry.attributes;

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

	int strideInFloats = 0;
	size_t strideInBytes = 0;
	for (const auto& attr : attributes)
	{
		strideInFloats += attr.size;
		strideInBytes += attr.size * ((attr.type == GL_INT) ? sizeof(int) : sizeof(float));
	}

	size_t offset = 0;
	for (const auto& attr : attributes)
	{
		glVertexAttribPointer(attr.index, attr.size, attr.type, attr.normalized, (GLsizei)strideInBytes, reinterpret_cast<void*>(offset));
		glEnableVertexAttribArray(attr.index);

		GLint typeSize = (attr.type == GL_INT) ? sizeof(int) : sizeof(float);
		offset += attr.size * typeSize;
	}

	indexCount = static_cast<GLsizei>(indices.size());
	glBindVertexArray(0);

	GLenum err = glGetError();
	if (err != GL_NO_ERROR) std::cerr << "GL error after mesh setup: " << err << "\n";
}

void Mesh::draw() const
{
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

void Mesh::drawInstanced(int instanceCount) const
{
	glBindVertexArray(VAO);
	glDrawElementsInstanced(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0, instanceCount);
	glBindVertexArray(0);
}


void Mesh::setInstanceTransforms(const std::vector<glm::mat4>& transforms)
{
	if (instanceVBO == 0)
		glGenBuffers(1, &instanceVBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

	size_t byteSize = transforms.size() * sizeof(glm::mat4);
	glBufferData(GL_ARRAY_BUFFER, byteSize, nullptr, GL_DYNAMIC_DRAW);
	glBufferSubData(GL_ARRAY_BUFFER, 0, byteSize, transforms.data());

	GLuint startLocation = 0;
	if (!attributes.empty())
	{
		const auto& lastAttr = attributes.back();
		startLocation = lastAttr.index + 1;
	}

	std::size_t vec4Size = sizeof(glm::vec4);
	for (unsigned int i = 0; i < 4; i++)
	{
		glEnableVertexAttribArray(startLocation + i);
		glVertexAttribPointer(startLocation + i, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4), (void*)(i * vec4Size));
		glVertexAttribDivisor(startLocation + i, 1);
	}

	glBindVertexArray(0);
}

Mesh::~Mesh()
{
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
	if (instanceVBO != 0) glDeleteBuffers(1, &instanceVBO);
}