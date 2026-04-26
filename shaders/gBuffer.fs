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
uniform bool hasHeightMap;
uniform bool hasARMMap;
uniform float height_scale;

uniform float aoBias;
uniform float roughnessBias;
uniform float metallicBias;

uniform vec3 viewPos;

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir);

void main()
{    
    vec3 N = normalize(fs_in.Normal);
	vec3 T = normalize(fs_in.Tangent);
	T = normalize(T - dot(T, N) * N);
	vec3 B = cross(T, N);
	mat3 TBN = mat3(T, B, N);

	vec3 norm = N;
	vec3 viewDir = normalize(viewPos - fs_in.FragPos);
	vec2 texCoords = fs_in.TexCoords;

	if (hasHeightMap)
	{
		viewDir = normalize(transpose(TBN) * viewDir);
		texCoords = ParallaxMapping(fs_in.TexCoords, viewDir);
	
		 // discards a fragment when sampling outside default texture region (fixes border artifacts)
		if(texCoords.x > 1.0 || texCoords.y > 1.0 || texCoords.x < 0.0 || texCoords.y < 0.0)
			discard;
	}

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

vec2 ParallaxMapping(vec2 texCoords, vec3 viewDir)
{
	// number of depth layers
    const float minLayers = 10;
    const float maxLayers = 20;
    float numLayers = mix(maxLayers, minLayers, abs(dot(vec3(0.0, 0.0, 1.0), viewDir)));  
    // calculate the size of each layer
    float layerDepth = 1.0 / numLayers;
    // depth of current layer
    float currentLayerDepth = 0.0;
    // the amount to shift the texture coordinates per layer (from vector P)
    vec2 P = viewDir.xy / viewDir.z * height_scale; 
    vec2 deltaTexCoords = P / numLayers;
  
    // get initial values
    vec2  currentTexCoords     = texCoords;
    float currentDepthMapValue = texture(height, currentTexCoords).r;
      
    while(currentLayerDepth < currentDepthMapValue)
    {
        // shift texture coordinates along direction of P
        currentTexCoords -= deltaTexCoords;
        // get depthmap value at current texture coordinates
        currentDepthMapValue = texture(height, currentTexCoords).r;  
        // get depth of next layer
        currentLayerDepth += layerDepth;  
    }
    
    // -- parallax occlusion mapping interpolation from here on
    // get texture coordinates before collision (reverse operations)
    vec2 prevTexCoords = currentTexCoords + deltaTexCoords;

    // get depth after and before collision for linear interpolation
    float afterDepth  = currentDepthMapValue - currentLayerDepth;
    float beforeDepth = texture(height, prevTexCoords).r - currentLayerDepth + layerDepth;
 
    // interpolation of texture coordinates
    float weight = afterDepth / (afterDepth - beforeDepth);
    vec2 finalTexCoords = prevTexCoords * weight + currentTexCoords * (1.0 - weight);

    return finalTexCoords;
}