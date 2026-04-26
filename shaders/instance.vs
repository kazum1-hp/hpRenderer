#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in mat4 instanceMVP;

out vec2 TexCoord;

//uniform mat4 view;
//uniform mat4 projection;

void main()
{
	gl_Position = instanceMVP * vec4(aPos, 1.0);

	TexCoord = aTexCoord;
}