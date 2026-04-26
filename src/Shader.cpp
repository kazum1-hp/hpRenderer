#include "../include/Shader.h"
#include <fstream>
#include <sstream>
#include <iostream>

Shader::Shader(const std::string& vertexPath, const std::string& fragmentPath, const std::string& geometryPath)
	:vertexPath(vertexPath), fragmentPath(fragmentPath), geometryPath(geometryPath)
{
	lastVertexWriteTime = std::filesystem::last_write_time(vertexPath);
	lastFragmentWriteTime = std::filesystem::last_write_time(fragmentPath);
	if (!geometryPath.empty()) {
		lastGeometryWriteTime = std::filesystem::last_write_time(geometryPath);
	}

	std::string vertexCode;
	std::string fragmentCode;
	std::string geometryCode;
	std::ifstream vShaderFile;
	std::ifstream fShaderFile;
	std::ifstream gShaderFile;

	vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	gShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

	try
	{
		vShaderFile.open(vertexPath);
		fShaderFile.open(fragmentPath);
		std::stringstream vShaderStream, fShaderStream;

		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();

		vShaderFile.close();
		fShaderFile.close();

		vertexCode = vShaderStream.str();
		fragmentCode = fShaderStream.str();
		
		if (!geometryPath.empty())
		{
			gShaderFile.open(geometryPath);
			std::stringstream gShaderStream;
			gShaderStream << gShaderFile.rdbuf();
			gShaderFile.close();
			geometryCode = gShaderStream.str();
		}
	}

	catch (std::ifstream::failure& e)
	{
		std::cerr << "ERROR::SHADER::FILE_NOT_SUCCESSFULLY_READ: " << e.what() << std::endl;
	}

	const char* vShaderCode = vertexCode.c_str();
	const char* fShaderCode = fragmentCode.c_str();

	unsigned int vertex = 0, fragment = 0, geometry = 0;

	vertex = glCreateShader(GL_VERTEX_SHADER);
	fragment = glCreateShader(GL_FRAGMENT_SHADER);

	glShaderSource(vertex, 1, &vShaderCode, NULL);
	glShaderSource(fragment, 1, &fShaderCode, NULL);

	glCompileShader(vertex);
	glCompileShader(fragment);

	checkCompileErrors(vertex, "VERTEX");
	checkCompileErrors(fragment, "FRAGMENT");

	if (!geometryCode.empty())
	{
		const char* gShaderCode = geometryCode.c_str();
		geometry = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(geometry, 1, &gShaderCode, nullptr);
		glCompileShader(geometry);
		checkCompileErrors(geometry, "GEOMETRY");
	}

	ID = glCreateProgram();
	glAttachShader(ID, vertex);
	glAttachShader(ID, fragment);
	if (!geometryCode.empty())
		glAttachShader(ID, geometry);

	glLinkProgram(ID);
	checkCompileErrors(ID, "PROGRAM");

	glDeleteShader(vertex);
	glDeleteShader(fragment);
	if (!geometryCode.empty())
		glDeleteShader(geometry);
}

void Shader::use() const{
    glUseProgram(ID);
}

void Shader::checkCompileErrors(unsigned int shader, std::string type)
{
	int success;
	char infoLog[1024];
	if (type != "PROGRAM")
	{
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success)
		{
			glGetShaderInfoLog(shader, 1024, NULL, infoLog);
			std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
		}
	}
	else
	{
		glGetProgramiv(shader, GL_LINK_STATUS, &success);
		if (!success)
		{
			glGetProgramInfoLog(shader, 1024, NULL, infoLog);
			std::cerr << "ERROR::PROGRAM_LINKING_ERROR of type: " << type << "\n" << infoLog << "\n -- --------------------------------------------------- -- " << std::endl;
		}
	}
}

// Added: Hot reload implementation
bool Shader::reload()
{
	// Check the last write time of the file, if any file has changed, attempt to reload.
	std::filesystem::file_time_type vtime, ftime, gtime;
	bool hasGeometry = !geometryPath.empty();
	try {
		vtime = std::filesystem::last_write_time(vertexPath);
		ftime = std::filesystem::last_write_time(fragmentPath);
		if (hasGeometry) gtime = std::filesystem::last_write_time(geometryPath);
	} catch (std::filesystem::filesystem_error& e) {
		std::cerr << "ERROR::SHADER::RELOAD::FILE_TIME: " << e.what() << std::endl;
		return false;
	}

	if (vtime == lastVertexWriteTime && ftime == lastFragmentWriteTime && (!hasGeometry || gtime == lastGeometryWriteTime)) {
		// Unmodified
		return false;
	}

	// Read new source code
	std::string vertexCode;
	std::string fragmentCode;
	std::string geometryCode;
	std::ifstream vShaderFile;
	std::ifstream fShaderFile;
	std::ifstream gShaderFile;

	try {
		vShaderFile.open(vertexPath);
		fShaderFile.open(fragmentPath);
		std::stringstream vShaderStream, fShaderStream;
		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();
		vShaderFile.close();
		fShaderFile.close();
		vertexCode = vShaderStream.str();
		fragmentCode = fShaderStream.str();

		if (hasGeometry) {
			gShaderFile.open(geometryPath);
			std::stringstream gShaderStream;
			gShaderStream << gShaderFile.rdbuf();
			gShaderFile.close();
			geometryCode = gShaderStream.str();
		}
	}
	catch (std::ifstream::failure& e)
	{
		std::cerr << "ERROR::SHADER::RELOAD::FILE_NOT_READ: " << e.what() << std::endl;
		return false;
	}

	const char* vShaderCode = vertexCode.c_str();
	const char* fShaderCode = fragmentCode.c_str();

	// Creating & compiling a new shader
	GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
	GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
	GLuint geometry = 0;

	glShaderSource(vertex, 1, &vShaderCode, nullptr);
	glCompileShader(vertex);
	GLint success = 0;
	glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
	if (!success) {
		char infoLog[1024];
		glGetShaderInfoLog(vertex, 1024, NULL, infoLog);
		std::cerr << "ERROR::SHADER::RELOAD::VERTEX_COMPILATION_FAILED\n" << infoLog << std::endl;
		glDeleteShader(vertex);
		return false;
	}

	glShaderSource(fragment, 1, &fShaderCode, nullptr);
	glCompileShader(fragment);
	glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
	if (!success) {
		char infoLog[1024];
		glGetShaderInfoLog(fragment, 1024, NULL, infoLog);
		std::cerr << "ERROR::SHADER::RELOAD::FRAGMENT_COMPILATION_FAILED\n" << infoLog << std::endl;
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		return false;
	}

	if (hasGeometry) {
		const char* gShaderCode = geometryCode.c_str();
		geometry = glCreateShader(GL_GEOMETRY_SHADER);
		glShaderSource(geometry, 1, &gShaderCode, nullptr);
		glCompileShader(geometry);
		glGetShaderiv(geometry, GL_COMPILE_STATUS, &success);
		if (!success) {
			char infoLog[1024];
			glGetShaderInfoLog(geometry, 1024, NULL, infoLog);
			std::cerr << "ERROR::SHADER::RELOAD::GEOMETRY_COMPILATION_FAILED\n" << infoLog << std::endl;
			glDeleteShader(vertex);
			glDeleteShader(fragment);
			glDeleteShader(geometry);
			return false;
		}
	}

	// Link to new program
	GLuint newProgram = glCreateProgram();
	glAttachShader(newProgram, vertex);
	glAttachShader(newProgram, fragment);
	if (hasGeometry) glAttachShader(newProgram, geometry);

	glLinkProgram(newProgram);
	glGetProgramiv(newProgram, GL_LINK_STATUS, &success);
	if (!success) {
		char infoLog[1024];
		glGetProgramInfoLog(newProgram, 1024, NULL, infoLog);
		std::cerr << "ERROR::SHADER::RELOAD::PROGRAM_LINKING_FAILED\n" << infoLog << std::endl;
		// Delete
		glDeleteShader(vertex);
		glDeleteShader(fragment);
		if (hasGeometry) glDeleteShader(geometry);
		glDeleteProgram(newProgram);
		return false;
	}

	// Link successful: Replace old program
	glDeleteProgram(ID); // Delete old program
	ID = newProgram;

	// Delete shaders (already attached and linked)
	glDeleteShader(vertex);
	glDeleteShader(fragment);
	if (hasGeometry) glDeleteShader(geometry);

	// Clear uniform cache and update write time
	uniformCache.clear();
	lastVertexWriteTime = vtime;
	lastFragmentWriteTime = ftime;
	if (hasGeometry) lastGeometryWriteTime = gtime;

	std::cout << "INFO::SHADER::RELOADED: " << vertexPath << " / " << fragmentPath << (hasGeometry ? (" / " + geometryPath) : "") << std::endl;
	return true;
}
