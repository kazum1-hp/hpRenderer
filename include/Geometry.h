#pragma once

#include <vector>
#include <glad/glad.h>

struct VertexAttribute
{
	GLuint index;
	GLint size;
	GLenum type;
	GLboolean normalized;
};

class Geometry
{
public:
	std::vector<float> vertices;
	std::vector<unsigned int> indices;
	std::vector<VertexAttribute> attributes;

	Geometry() = default;

	Geometry(const std::vector<float>& verts, const std::vector<unsigned int>& i, const std::vector<VertexAttribute>& attrs)
		: vertices(verts), indices(i), attributes(attrs) {
	}
};
