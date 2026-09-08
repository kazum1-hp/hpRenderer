#version 330 core
in vec2 ShadowUV;
uniform sampler2D diffuse;
uniform sampler2D opacityMap;
uniform bool hasDiffuseMap;
uniform bool hasOpacityMap;
uniform vec4 baseColorFactor;
uniform int alphaMode;
uniform float alphaCutoff;
in vec4 FragPos;

uniform vec3 lightPos;
uniform float far_plane;

void main()
{
    float alpha = baseColorFactor.a;
    if (hasDiffuseMap) alpha *= texture(diffuse, ShadowUV).a;
    if (hasOpacityMap) alpha *= texture(opacityMap, ShadowUV).r;
    if (alphaMode == 1 && alpha < alphaCutoff) discard;
    // get distance between fragment and light source
    float lightDistance = length(FragPos.xyz - lightPos);

    // map to [0;1] range by dividing by far_plane
    lightDistance = lightDistance / far_plane;

    // write this as modified depth
    gl_FragDepth = lightDistance;
}
