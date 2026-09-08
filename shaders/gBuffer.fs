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
uniform vec4 baseColorFactor;
uniform float roughnessFactor;
uniform float metallicFactor;
uniform int alphaMode;
uniform float alphaCutoff;
uniform bool doubleSided;
uniform bool hasDiffuseMap;
uniform bool hasOpacityMap;
uniform bool hasAOMap;
uniform bool hasRoughnessMap;
uniform bool hasMetallicMap;
uniform sampler2D opacityMap;
uniform sampler2D aoMap;
uniform sampler2D roughnessMap;
uniform sampler2D metallicMap;
uniform int roughnessChannel;
uniform int metallicChannel;

uniform bool hasNormalMap;
uniform bool hasARMMap;

uniform float aoBias;
uniform float roughnessBias;
uniform float metallicBias;

uniform vec3 viewPos;

vec4 surfaceColor(vec2 uv)
{
    vec4 color = baseColorFactor * (hasDiffuseMap ? texture(diffuse, uv) : vec4(1.0));
    if (hasOpacityMap) color.a *= texture(opacityMap, uv).r;
    if (alphaMode == 1 && color.a < alphaCutoff) discard;
    if (alphaMode == 2 && color.a <= 0.001) discard;
    if (alphaMode != 2) color.a = 1.0;
    return color;
}

vec3 surfaceARM(vec2 uv)
{
    vec3 value = vec3(1.0, roughnessFactor, metallicFactor);
    if (hasARMMap) value = texture(arm, uv).rgb; // Legacy built-in packed ARM.
    if (hasAOMap) value.r = texture(aoMap, uv).r;
    if (hasRoughnessMap) value.g = texture(roughnessMap, uv)[roughnessChannel] * roughnessFactor;
    if (hasMetallicMap) value.b = texture(metallicMap, uv)[metallicChannel] * metallicFactor;
    return clamp(value + vec3(aoBias, roughnessBias, metallicBias), vec3(0.0, 0.04, 0.0), vec3(1.0));
}

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

    if (doubleSided && !gl_FrontFacing) { N = -N; norm = -norm; }
    // position
    gPosition = fs_in.FragPos;
    // normal (used for lighting)
    gNormal = norm;
    // albedo
    gAlbedo = surfaceColor(texCoords).rgb;

    gARM = surfaceARM(texCoords);

    // shadow calculate normal
    gGeoNormal = N;
}
