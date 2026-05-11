#version 330 core
layout (location = 0) out vec3 gPosition;
layout (location = 1) out vec3 gNormal;
layout (location = 2) out vec3 gAlbedo;
layout (location = 3) out vec3 gARM; // occlusion, roughness, metallic
layout (location = 4) out vec3 gGeoNormal;

in VS_OUT{
	vec2 TexCoords;
	vec3 Normal;
	vec3 FragPos;
	vec3 Tangent;
	vec3 Bitangent;
} fs_in;

uniform sampler2D diffuse;
uniform sampler2D specular;
uniform sampler2D normal;
uniform sampler2D height;
uniform sampler2D arm;

uniform bool hasNormalMap;
uniform bool hasARMMap;

uniform float aoBias;
uniform float roughnessBias;
uniform float metallicBias;

uniform vec3 viewPos;

void main()
{    
    vec3 N = normalize(fs_in.Normal);
	vec3 T = normalize(fs_in.Tangent);
	T = normalize(T - dot(T, N) * N);
	vec3 B = normalize(fs_in.Bitangent);
    B = normalize(B - dot(B,N)*N);
    B = normalize(B - dot(B,T)*T);
	mat3 TBN = mat3(T, B, N);

	vec3 norm = N;
	vec3 viewDir = normalize(viewPos - fs_in.FragPos);
	vec2 texCoords = fs_in.TexCoords;

	if (hasNormalMap)
	{
		vec3 normalTex = texture(normal, texCoords).xyz;
		normalTex = normalTex * 2.0 - 1.0;
		// If the Y channel is reversed:
		//normalTex.y = -normalTex.y;
		norm = normalize(TBN * normalTex);
	}

    // position
    gPosition = fs_in.FragPos;
    // normal (used for lighting)
    gNormal = norm;
    // albedo
    gAlbedo = texture(diffuse, texCoords).rgb;

    // ORM: occlusion, roughness, metallic
    float ao = 1.0;
    float roughness = 0.5;
    float metallic = 0.0;

    if (hasARMMap)
    {
        vec3 armVal = texture(arm, texCoords).rgb;
        ao = clamp(armVal.r + aoBias, 0.01, 1.0);
        roughness = clamp(armVal.g + roughnessBias, 0.01, 1.0);
        metallic = clamp(armVal.b + metallicBias, 0.01, 1.0);
    }

    gARM = vec3(ao, roughness, metallic);

    // shadow calculate normal
    gGeoNormal = N;
}