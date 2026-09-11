#version 330 core
layout (location = 0) out vec4 FragColor;
layout (location = 1) out vec4 BrightColor;
in vec3 WorldPos;

uniform samplerCube environmentMap;


void main()
{		
    vec3 envColor = texture(environmentMap, WorldPos).rgb;
    
    // Display conversion is performed once, after scene composition.
    
    FragColor = vec4(envColor, 1.0);
    BrightColor = vec4(0.0, 0.0, 0.0, 1.0);
}
