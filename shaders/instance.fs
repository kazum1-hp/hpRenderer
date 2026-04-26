#version 330 core
in vec2 TexCoord;

out vec4 FragColor;

struct Material {
	sampler2D diffuse;
	sampler2D specular;
	float shininess;
};

uniform Material material;

void main()
{	
	vec4 baseDiffuse = texture(material.diffuse, TexCoord);
	vec4 baseSpecular = texture(material.specular, TexCoord);

	vec3 textureColor = baseDiffuse.rgb + baseSpecular.rgb; 
	float alpha = baseDiffuse.a; 

	if (alpha < 0.1)
		discard;

	FragColor = vec4(textureColor, alpha);
}