#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

//out vec4 FragColor;
in vec2 TexCoords;

struct ParallelLight {
	vec3 direction;
    vec3 color;
    float intensity;

	bool enabled;
};

struct PointLight {
	vec3 position;  
    vec3 color;
    float intensity;

	float constant;
    float linear;
    float quadratic;

	bool enabled;
};

uniform ParallelLight parallelLight;
uniform PointLight pointLight[4];

uniform sampler2D depthMap;
uniform samplerCube shadowMap[4];

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gARM; // ao,roughness,metallic
uniform sampler2D gGeoNormal;
uniform sampler2D gDepth;

uniform samplerCube irradianceMap;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

uniform mat4 lightSpaceMatrix;

uniform vec3 viewPos;
uniform bool useQuadratic;

uniform float far_plane;
uniform bool parallelShadows;
uniform bool pointShadows;

const float PI = 3.14159265359;
// array of offset direction for sampling
vec3 gridSamplingDisk[20] = vec3[]
(
   vec3(1, 1,  1), vec3( 1, -1,  1), vec3(-1, -1,  1), vec3(-1, 1,  1), 
   vec3(1, 1, -1), vec3( 1, -1, -1), vec3(-1, -1, -1), vec3(-1, 1, -1),
   vec3(1, 1,  0), vec3( 1, -1,  0), vec3(-1, -1,  0), vec3(-1, 1,  0),
   vec3(1, 0,  1), vec3(-1,  0,  1), vec3( 1,  0, -1), vec3(-1, 0, -1),
   vec3(0, 1,  1), vec3( 0, -1,  1), vec3( 0, -1, -1), vec3( 0, 1, -1)
);

float DistributionGGX(vec3 N, vec3 H, float roughness);
float GeometrySchlickGGX(float NdotV, float roughness);
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness);
vec3 fresnelSchlick(float cosTheta, vec3 F0);
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness);

vec3 CalParallelLight(ParallelLight parallelLight, vec3 norm, vec3 viewDir, vec3 parallelLightDir, vec3 texColor, float parallelShadow, float roughness, float metallic);
vec3 CalPointLight(PointLight pointLight, vec3 FragPos, vec3 norm, vec3 viewDir, vec3 pointLightDir, vec3 texColor, float pointShadow, float roughness, float metallic);

float ShadowCalculation(vec4 FragPosLightSpace, vec3 n);
float PointShadowCalculation(vec3 fragPos, vec3 normal, PointLight pointLight, samplerCube shadowMap);

void main()
{
	float depth = texture(gDepth, TexCoords).r;

	if (depth >= 0.9999)
	{
		discard; // clear background color
	}

	vec3 FragPos = texture(gPosition, TexCoords).rgb;
    vec3 norm = normalize(texture(gNormal, TexCoords).rgb);
    vec3 albedo = texture(gAlbedo, TexCoords).rgb;
	vec3 arm = texture(gARM, TexCoords).rgb;
	float ao = arm.r;
	float roughness = arm.g;
	float metallic = arm.b;
	vec3 N = texture(gGeoNormal, TexCoords).rgb;

	vec3 viewDir  = normalize(viewPos - FragPos);
	vec4 FragPosLightSpace = lightSpaceMatrix * vec4(FragPos, 1.0);

	vec3 pointColor = vec3(0.0);

	for (int i = 0; i < 4; ++i)
	{
		vec3 pointLightDir = normalize(pointLight[i].position - FragPos);

		float pointShadow = pointShadows ? PointShadowCalculation(FragPos, N, pointLight[i], shadowMap[i]) : 0.0;

		pointColor += CalPointLight(pointLight[i], FragPos, norm, viewDir, pointLightDir, albedo, pointShadow, roughness, metallic);
	}

	vec3 parallelLightDir = normalize(-parallelLight.direction);
	float parallelShadow = parallelShadows ? ShadowCalculation(FragPosLightSpace, N) : 0.0;
	vec3 parallelColor = CalParallelLight(parallelLight, norm, viewDir, parallelLightDir, albedo, parallelShadow, roughness, metallic);

    vec3 ambient = vec3(0.0);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 kS = fresnelSchlickRoughness(max(dot(norm, viewDir), 0.0), F0, roughness);
    vec3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    vec3 irradiance = texture(irradianceMap, norm).rgb;
    vec3 amDiffuse  = irradiance * albedo;

    // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
    const float MAX_REFLECTION_LOD = 10.0;
    vec3 R = reflect(-viewDir, norm);
    vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;    
    vec2 brdf  = texture(brdfLUT, vec2(max(dot(norm, viewDir), 0.0), roughness)).rg;
    vec3 amSpecular = prefilteredColor * (kS * brdf.x + brdf.y);

    ambient = (kD * amDiffuse + amSpecular) * ao;

	vec3 textureColor = pointColor + parallelColor + ambient;

    BrightColor = vec4(0.0, 0.0, 0.0, 1.0);

    FragColor = vec4(textureColor, 1.0);
}

// parallelLight
vec3 CalParallelLight(ParallelLight parallelLight, vec3 norm, vec3 viewDir, vec3 parallelLightDir, vec3 texColor, float parallelShadow, float roughness, float metallic)
{
	//vec3 ambient = (parallelLight.ambient) * texColor * ao;

    vec3 parallelHalfVec = normalize(parallelLightDir + viewDir);
    vec3 radiance = parallelLight.color * parallelLight.intensity;

    vec3 F0 = vec3(0.04); 
    vec3 albedo = texColor.rgb;
    F0 = mix(F0, albedo, metallic);

    float NDF = DistributionGGX(norm, parallelHalfVec, roughness);
    float G   = GeometrySmith(norm, viewDir, parallelLightDir, roughness);
    vec3 F    = fresnelSchlick(clamp(dot(parallelHalfVec, viewDir), 0.0, 1.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(norm, viewDir), 0.0) * max(dot(norm, parallelLightDir), 0.0) + 0.0001; // prevent divide by zero
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(norm, parallelLightDir), 0.0);
    vec3 diffuse = kD * texColor / PI;

    vec3 Lo = (diffuse + specular) * radiance * NdotL;

    vec3 parallelLightColor = parallelLight.enabled? ((1 - parallelShadow) * Lo) : vec3(0.0);

    return parallelLightColor;
}

float ShadowCalculation(vec4 FragPosLightSpace, vec3 n)
{
	// perform perspective divide
    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    float closestDepth = texture(depthMap, projCoords.xy).r; 
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // calculate bias (based on depth map resolution and slope)
    vec3 lightDir = normalize(-parallelLight.direction); 
    float bias = max(0.01 * (1.0 - dot(n, lightDir)), 0.001);
    // check whether current frag pos is in shadow
    // float shadow = currentDepth - bias > closestDepth  ? 1.0 : 0.0;
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(depthMap, 0);
    for(int x = -1; x <= 1; ++x)
    {
        for(int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(depthMap, projCoords.xy + vec2(x, y) * texelSize).r; 
            shadow += currentDepth - bias > pcfDepth  ? 1.0 : 0.0;        
        }    
    }
    shadow /= 9.0;
    
    // keep the shadow at 0.0 when outside the far_plane region of the light's frustum.
    if(projCoords.z > 1.0)
        shadow = 0.0;
        
    return shadow;
}

// pointLight	
vec3 CalPointLight(PointLight pointLight, vec3 FragPos, vec3 norm, vec3 viewDir, vec3 pointLightDir, vec3 texColor, float pointShadow, float roughness, float metallic)
{
	float distance = length(pointLight.position - FragPos);
	float attenuation;
	if (useQuadratic)
		attenuation = 1.0 / (pointLight.constant + pointLight.linear * distance + pointLight.quadratic * (distance * distance));
	else
		attenuation = 1.0 / (pointLight.constant + pointLight.linear * distance);
	
    vec3 pointHalfVec = normalize(pointLightDir + viewDir);
    vec3 radiance = pointLight.color * pointLight.intensity * attenuation;

    vec3 F0 = vec3(0.04); 
    vec3 albedo = texColor.rgb;
    F0 = mix(F0, albedo, metallic);

    float NDF = DistributionGGX(norm, pointHalfVec, roughness);
    float G   = GeometrySmith(norm, viewDir, pointLightDir, roughness);
    vec3 F    = fresnelSchlick(clamp(dot(pointHalfVec, viewDir), 0.0, 1.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(norm, viewDir), 0.0) * max(dot(norm, pointLightDir), 0.0) + 0.0001; // prevent divide by zero
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(norm, pointLightDir), 0.0);
    vec3 diffuse = kD * texColor / PI;

    vec3 Lo = (diffuse + specular) * radiance * NdotL;

	//vec3 ambient = (attenuation) *  (pointLight.ambient) * texColor * ao;

    vec3 pointLightColor = pointLight.enabled? ((1 - pointShadow) * Lo) : vec3(0.0);

    return pointLightColor;
}

float PointShadowCalculation(vec3 fragPos, vec3 normal, PointLight pointLight, samplerCube shadowMap)
{
    vec3 fragToLight = fragPos - pointLight.position;
    float currentDepth = length(fragToLight);

    float shadow = 0.0;
    int samples = 20;

    vec3 lightDir = normalize(pointLight.position - fragPos);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    float diskRadius = clamp(0.02 * currentDepth / far_plane, 0.001, 0.02);

    for(int i = 0; i < samples; ++i)
    {
        vec3 sampleDir = normalize(fragToLight + normalize(gridSamplingDisk[i]) * diskRadius);

        float closestDepth = texture(shadowMap, sampleDir).r;
        closestDepth *= far_plane;

        if(currentDepth > closestDepth + bias)
            shadow += 1.0;
    }

    shadow /= float(samples);
    return shadow;
}

// ----------------------------------------------------------------------------
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}   