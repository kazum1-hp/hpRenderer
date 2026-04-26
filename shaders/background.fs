#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;
in vec3 WorldPos;

uniform samplerCube environmentMap;

uniform bool usePost;

void main()
{		
    vec3 envColor = texture(environmentMap, WorldPos).rgb;
    
    if (!usePost)
    {   // HDR tonemap and gamma correct
        envColor = envColor / (envColor + vec3(1.0));
        envColor = pow(envColor, vec3(1.0/2.2));
    }
    
    FragColor = vec4(envColor, 1.0);
    BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}