#version 330 core
in vec2 ShadowUV;
uniform sampler2D diffuse;
uniform sampler2D opacityMap;
uniform bool hasDiffuseMap;
uniform bool hasOpacityMap;
uniform vec4 baseColorFactor;
uniform int alphaMode;
uniform float alphaCutoff;

void main()
{
    float alpha = baseColorFactor.a;
    if (hasDiffuseMap) alpha *= texture(diffuse, ShadowUV).a;
    if (hasOpacityMap) alpha *= texture(opacityMap, ShadowUV).r;
    if (alphaMode == 1 && alpha < alphaCutoff) discard;
    // gl_FragDepth = gl_FragCoord.z;
}
