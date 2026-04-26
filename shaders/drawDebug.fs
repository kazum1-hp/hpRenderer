#version 330 core

uniform sampler2D debugTex;
uniform int u_DebugMode;

uniform float fov;
uniform float nearPlane;
uniform float farPlane;

in vec2 TexCoords;
out vec4 FragColor;

float LinearizeDepth(float depth);

void main()
{
    if (u_DebugMode == 0)
    {
        // gNormal£¨World Space ¡ú mapping to [0,1]£©
        vec3 normal = texture(debugTex, TexCoords).rgb;
        normal = normalize(normal);
        FragColor = vec4(normal * 0.5 + 0.5, 1.0);
    }
    else if (u_DebugMode == 1)
    {
        // Roughness
        float roughness = texture(debugTex, TexCoords).g;
        FragColor = vec4(vec3(roughness), 1.0);
    }
    else if (u_DebugMode == 2)
    {
        // Metallic
        float metallic = texture(debugTex, TexCoords).b;
        FragColor = vec4(vec3(metallic), 1.0);
    }
    else if (u_DebugMode == 3)
    {
        // Depth
        float d = texture(debugTex, TexCoords).r;
        float linear = LinearizeDepth(d) / farPlane;
        FragColor = vec4(vec3(linear), 1.0);
    }
    else
    {
        FragColor = vec4(1.0, 0.0, 1.0, 1.0); // fallback
    }
}

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // [0,1] ¡ú NDC [-1,1]
    return (2.0 * nearPlane * farPlane) /
           (farPlane + nearPlane - z * (farPlane - nearPlane));
}
