#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;

const int MAX_POINT_LIGHTS = 4;

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
	float farPlane;

	bool enabled;
};

uniform ParallelLight parallelLight;
uniform PointLight pointLight[MAX_POINT_LIGHTS];
uniform int pointLightCount;

uniform sampler2D depthMap;
uniform samplerCube shadowMap[MAX_POINT_LIGHTS];

uniform sampler2D gPosition;
uniform sampler2D gNormal;
uniform sampler2D gAlbedo;
uniform sampler2D gARM; // ao,roughness,metallic
uniform sampler2D gGeoNormal;
uniform sampler2D gDepth;

uniform samplerCube irradianceMap;
uniform bool useIBL;
uniform bool useHemisphere;
uniform float hemisphereIntensity;
uniform float iblIntensity;
uniform vec3 ambientSkyColor;
uniform vec3 ambientGroundColor;
uniform samplerCube prefilterMap;
uniform sampler2D brdfLUT;

uniform mat4 lightSpaceMatrix;
uniform mat4 inverseViewProjection;

uniform vec3 viewPos;
uniform bool useQuadratic;

uniform bool parallelShadows;
uniform float directionalShadowInvDepthRange;
uniform float directionalShadowDistance;
uniform vec4 directionalShadowCameraDepth;
uniform mat3 directionalShadowNormalMatrix;
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

float ShadowCalculation(vec4 FragPosLightSpace, vec3 worldPos);
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
    // RGBA16F world positions lose sub-texel precision far from the origin.
    // Reconstruct the shadow receiver from the 24-bit depth buffer instead.
    vec4 shadowWorld = inverseViewProjection * vec4(TexCoords * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec3 shadowWorldPos = shadowWorld.xyz / shadowWorld.w;
	vec4 FragPosLightSpace = lightSpaceMatrix * vec4(shadowWorldPos, 1.0);

	vec3 pointColor = vec3(0.0);

	for (int i = 0; i < MAX_POINT_LIGHTS; ++i)
	{
		if (i >= pointLightCount) break;
		if (!pointLight[i].enabled) continue;
		vec3 pointLightDir = normalize(pointLight[i].position - FragPos);

		float pointShadow = pointShadows ? PointShadowCalculation(FragPos, N, pointLight[i], shadowMap[i]) : 0.0;

		pointColor += CalPointLight(pointLight[i], FragPos, norm, viewDir, pointLightDir, albedo, pointShadow, roughness, metallic);
	}

	vec3 parallelLightDir = normalize(-parallelLight.direction);
	float parallelShadow = parallelShadows ? ShadowCalculation(FragPosLightSpace, shadowWorldPos) : 0.0;
	vec3 parallelColor = CalParallelLight(parallelLight, norm, viewDir, parallelLightDir, albedo, parallelShadow, roughness, metallic);

    vec3 ambient = vec3(0.0);
    if (useHemisphere)
    {
        float skyWeight = clamp(norm.y * 0.5 + 0.5, 0.0, 1.0);
        vec3 hemisphere = mix(ambientGroundColor, ambientSkyColor, skyWeight);
        ambient = hemisphere * max(hemisphereIntensity, 0.0) * albedo * (1.0 - metallic) * ao;
    }

    if (useIBL)
    {
        vec3 F0 = vec3(0.04);
        F0 = mix(F0, albedo, metallic);

        vec3 kS = fresnelSchlickRoughness(max(dot(norm, viewDir), 0.0), F0, roughness);
        vec3 kD = 1.0 - kS;
        kD *= 1.0 - metallic;

        vec3 irradiance = texture(irradianceMap, norm).rgb;
        vec3 amDiffuse  = irradiance * albedo;

        // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
        const float MAX_REFLECTION_LOD = 4.0;
        vec3 R = reflect(-viewDir, norm);
        vec3 prefilteredColor = textureLod(prefilterMap, R,  roughness * MAX_REFLECTION_LOD).rgb;
        vec2 brdf  = texture(brdfLUT, vec2(max(dot(norm, viewDir), 0.0), roughness)).rg;
        vec3 amSpecular = prefilteredColor * (kS * brdf.x + brdf.y);

        ambient = (kD * amDiffuse + amSpecular) * ao * max(iblIntensity, 0.0);
    }

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

float ShadowCalculation(vec4 FragPosLightSpace, vec3 worldPos)
{
    vec3 projCoords = FragPosLightSpace.xyz / FragPosLightSpace.w * 0.5 + 0.5;
    // Differentiate world positions before projection: differences between
    // nearly equal shadow UVs lose precision when the coverage is very large.
    // Use the geometric plane, independent of interpolated/normal-map normals.
    vec3 planeNormal = directionalShadowNormalMatrix * cross(dFdx(worldPos), dFdy(worldPos));
    vec2 depthGradient = vec2(0.0);
    if (abs(planeNormal.z) > 1e-12)
        depthGradient = -planeNormal.xy / planeNormal.z;

    float cameraDepth = dot(directionalShadowCameraDepth, vec4(worldPos, 1.0));
    float endDistance = max(directionalShadowDistance, 0.001);
    float visibility = 1.0 - smoothstep(0.8 * endDistance, endDistance, cameraDepth);
    if (visibility <= 0.0 || any(lessThan(projCoords, vec3(0.0))) ||
        any(greaterThan(projCoords, vec3(1.0))))
        return 0.0;

    ivec2 mapSize = textureSize(depthMap, 0);
    vec2 texelSize = 1.0 / vec2(mapSize);
    vec2 edgeDistance = min(projCoords.xy, 1.0 - projCoords.xy);
    visibility *= smoothstep(0.0, 3.0 * max(texelSize.x, texelSize.y),
                             min(edgeDistance.x, edgeDistance.y));

    // Only a small residual offset is needed for floating-point/rasterization
    // error. The receiver-plane correction handles slope and PCF footprint.
    float bias = max(0.001 * directionalShadowInvDepthRange, 0.000002);
    // Rasterization quantizes projected vertices to a subpixel grid. Its
    // residual plane error grows with depth change per shadow texel.
    bias += 0.01 * dot(abs(depthGradient), texelSize);
    ivec2 centerTexel = ivec2(floor(projCoords.xy * vec2(mapSize)));
    float shadow = 0.0;
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            ivec2 sampleTexel = centerTexel + ivec2(x, y);
            if (any(lessThan(sampleTexel, ivec2(0))) ||
                any(greaterThanEqual(sampleTexel, mapSize)))
                continue;
            // Compare at the actual texel center, including nearest-sampling
            // quantization, instead of testing every tap against the same Z.
            vec2 sampleUV = (vec2(sampleTexel) + 0.5) * texelSize;
            float receiverDepth = projCoords.z + dot(depthGradient, sampleUV - projCoords.xy);
            float storedDepth = texelFetch(depthMap, sampleTexel, 0).r;
            if (storedDepth < 1.0 && receiverDepth - bias > storedDepth)
                shadow += 1.0;
        }
    }
    return visibility * shadow / 9.0;
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

    float diskRadius = clamp(0.02 * currentDepth / pointLight.farPlane, 0.001, 0.02);

    for(int i = 0; i < samples; ++i)
    {
        vec3 sampleDir = normalize(fragToLight + normalize(gridSamplingDisk[i]) * diskRadius);

        float closestDepth = texture(shadowMap, sampleDir).r;
        closestDepth *= pointLight.farPlane;

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
