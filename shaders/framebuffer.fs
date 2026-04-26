#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
uniform sampler2D screenTexture;
uniform sampler2D blur;
uniform int effectMode;
uniform float offset;
uniform float scanPos;
uniform bool useGamma;

uniform bool useHdr;
uniform bool useBloom;
uniform float exposure;

float gamma = 2.2;

float sharpnKernel[9] = float[](
        -1, -1, -1,
        -1,  9, -1,
        -1, -1, -1
    );

float blurKernel[9] = float[](
    2.0 / 18, 2.0 / 18, 2.0 / 18,
    2.0 / 18, 2.0 / 18, 2.0 / 18,
    2.0 / 18, 2.0 / 18, 2.0 / 18  
);

vec2 offsets[9] = vec2[](
    vec2(-1.0 / offset, 1.0 / offset), // left top
    vec2( 0.0f, 1.0 / offset), // top
    vec2( 1.0 / offset, 1.0 / offset), // right top
    vec2(-1.0 / offset, 0.0f),   // left
    vec2( 0.0f, 0.0f),   // middle
    vec2( 1.0 / offset, 0.0f),   // right
    vec2(-1.0 / offset, -1.0 / offset), // left bottom
    vec2( 0.0f, -1.0 / offset), // bottom
    vec2( 1.0 / offset, -1.0 / offset)  // right bottom
);

void main()
{
    vec3 color = vec3(0.0);

    vec3 sampleTex[9];
        for(int i = 0; i < 9; i++)
        {
            sampleTex[i] = texture(screenTexture, TexCoords.st + offsets[i]).rgb;
        }

 if (gl_FragCoord.x > scanPos)
    {
        if (effectMode == 0) { color = texture(screenTexture, TexCoords).rgb; }

        else if (effectMode == 1) { color = 1 - texture(screenTexture, TexCoords).rgb; }

        else if (effectMode == 2) 
        {
            color = texture(screenTexture, TexCoords).rgb;
            float average = 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
            color = vec3(average);
        }

        else if (effectMode == 3)
        {  
            for(int i = 0; i < 9; i++)
                color += sampleTex[i] * sharpnKernel[i];
        }

        else if (effectMode == 4)
        {
            for(int i = 0; i < 9; i++)
                color += sampleTex[i] * blurKernel[i];
        }

        vec3 result;
        vec3 bloomColor = vec3(0.0);

        if (useHdr)
        {
            if (useBloom)
                bloomColor = texture(blur, TexCoords).rgb;
                color += bloomColor;
            // reinhard
            // result = color / (color + vec3(1.0));
            // exposure
            result = vec3(1.0) - exp(-color * exposure);
        }
        else
            result = color;

        if (useGamma)
            FragColor = vec4(pow(result, vec3(1.0 / gamma)), 1.0);
        else
            FragColor = vec4(result, 1.0);
    }
        
    else
        {
            color = vec3(texture(screenTexture, TexCoords).rgb);
            FragColor = vec4(pow(color, vec3(1.0 / gamma)), 1.0);
        }
}
