#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;
layout (location = 3) in vec3 aTangent;
layout (location = 4) in vec3 aBitangent;

out VS_OUT {
	vec2 TexCoords;
	vec3 Normal;
	vec3 FragPos;
	vec3 Tangent;
	vec3 Bitangent;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 worldPos = model * vec4(aPos, 1.0);
    vs_out.FragPos = worldPos.xyz; 
    vs_out.TexCoords = aTexCoords;
    
    mat3 normalMatrix = transpose(inverse(mat3(model)));
	vs_out.Normal = normalMatrix * aNormal;

    vs_out.Tangent = vec3(normalMatrix * aTangent);
	vs_out.Bitangent = vec3(normalMatrix * aBitangent);

    gl_Position = projection * view * worldPos;
}