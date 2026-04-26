#version 330 core
layout (location = 0) in vec3 aPos;

out vec3 WorldPos;

uniform mat4 projView;

void main()
{
    WorldPos = aPos;  
    gl_Position =  projView * vec4(WorldPos, 1.0);
}