#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

struct Material {
	sampler2D diffuse;
	sampler2D specular;
	float shininess;
};

uniform Material material;
uniform float modelLight;

void main()
{
	vec4 baseDiffuse = texture(material.diffuse, TexCoords);
	vec4 baseSpecular = texture(material.specular, TexCoords);

	vec3 textureColor = (baseDiffuse.rgb + baseSpecular.rgb) * modelLight; 
	float alpha = baseDiffuse.a; 

	if (alpha < 0.1)
		discard;

	FragColor = vec4(textureColor, alpha);
}