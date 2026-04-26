#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoord;
layout(location = 3) in vec3 aTangent;
layout(location = 4) in vec3 aBitangent;

out VS_OUT {
	vec2 TexCoords;
	vec3 Normal;
	vec3 FragPos;
	vec4 FragPosLightSpace;
	vec3 Tangent;
	vec3 Bitangent;
} vs_out;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

uniform mat4 lightSpaceMatrix;

void main()
{
    vs_out.FragPos = vec3(model * vec4(aPos, 1.0));

	vs_out.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out.FragPos, 1.0);

	mat3 normalMatrix = transpose(inverse(mat3(model)));
	vs_out.Normal = normalMatrix * aNormal;
	
	vs_out.Tangent = vec3(normalMatrix * aTangent);
	vs_out.Bitangent = vec3(normalMatrix * aBitangent);

    vs_out.TexCoords = aTexCoord;

	gl_Position = projection * view  * model * vec4(aPos, 1.0);
}
