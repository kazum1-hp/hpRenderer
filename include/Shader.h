#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

class Shader
{
public:
	unsigned int ID;

	Shader(const std::string& vertexPath, const std::string& fragmentPath, const std::string& geometryPath = "");

	void use() const;

	template<typename T>
	void setUniform(const std::string& name, const T& value) const;

	const std::string& GetVertexPath() const { return vertexPath; }
	const std::string& GetFragmentPath() const { return fragmentPath; }
	const std::string& GetGeometryPath() const { return geometryPath; }

	std::filesystem::file_time_type GetLastVertexWriteTime() const { return lastVertexWriteTime; }
	std::filesystem::file_time_type GetLastFragmentWriteTime() const { return lastFragmentWriteTime; }
	std::filesystem::file_time_type GetLastGeometryWriteTime() const { return lastGeometryWriteTime; }

	void updateWriteTimes() {
		lastVertexWriteTime = std::filesystem::last_write_time(vertexPath);
		lastFragmentWriteTime = std::filesystem::last_write_time(fragmentPath);
		lastGeometryWriteTime= std::filesystem::last_write_time(geometryPath);
	}

	// Added: Attempt hot reload;
	// returns true (replaces the program) if successful;
	// returns false if it fails or no changes are made.
	bool reload();

private:
	std::string vertexPath;
	std::string fragmentPath;
	std::string geometryPath;

	std::filesystem::file_time_type lastVertexWriteTime;
	std::filesystem::file_time_type lastFragmentWriteTime;
	std::filesystem::file_time_type lastGeometryWriteTime;

	void checkCompileErrors(unsigned int shader, std::string type);
	
	//Cache glGetUniformLocation
	mutable std::unordered_map<std::string, GLint> uniformCache;

	GLint getUniformLocation(const std::string& name) const
	{
		if (uniformCache.find(name) != uniformCache.end())
			return uniformCache.at(name);

		GLint location = glGetUniformLocation(ID, name.c_str());
		uniformCache[name] = location;
		return location;
	}
};

template<>
inline void Shader::setUniform<int>(const std::string& name, const int& value) const {
	glUniform1i(getUniformLocation(name), value);
}

template<>
inline void Shader::setUniform<float>(const std::string& name, const float& value) const {
	glUniform1f(getUniformLocation(name), value);
}

template<>
inline void Shader::setUniform<glm::vec2>(const std::string& name, const glm::vec2& value) const {
	glUniform2fv(getUniformLocation(name), 1, &value[0]);
}

template<>
inline void Shader::setUniform<glm::vec3>(const std::string& name, const glm::vec3& value) const {
	glUniform3fv(getUniformLocation(name), 1, &value[0]);
}

template<>
inline void Shader::setUniform<glm::mat3>(const std::string& name, const glm::mat3& value) const {
	glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

template<>
inline void Shader::setUniform<glm::mat4>(const std::string& name, const glm::mat4& value) const {
	glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
}

template<>
inline void Shader::setUniform<bool>(const std::string& name, const bool& value) const {
	glUniform1i(getUniformLocation(name), value ? 1 : 0);
}
