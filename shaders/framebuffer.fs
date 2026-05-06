#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D screenTexture;
uniform sampler2D blur;
uniform int effectMode;
uniform int toneMappingMode;
uniform float offset;
uniform float scanPos;

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

vec3 ACESFilm(vec3 x);
vec3 Uncharted2Tonemap(vec3 x);

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

            color *= exposure;

            // reinhard
            if (toneMappingMode == 0)    
                result = color / (color + vec3(1.0));

            // simple exposure
            else if (toneMappingMode == 1)   
                result = vec3(1.0) - exp(-color);
                
            // ACESFilm
            else if (toneMappingMode == 2)
                result = ACESFilm(color * 0.5);

            // Hable
            else if (toneMappingMode == 3)    
            {
                result = Uncharted2Tonemap(color);

                float W = 11.2;
                vec3 whiteScale = 1.0 / Uncharted2Tonemap(vec3(W));
                result *= whiteScale;
            }
        }
        else
            result = color;

        FragColor = vec4(pow(result, vec3(1.0 / gamma)), 1.0);
    }
        
    else
        {
            color = vec3(texture(screenTexture, TexCoords).rgb);
            FragColor = vec4(pow(color, vec3(1.0 / gamma)), 1.0);
        }
}

vec3 ACESFilm(vec3 x)
{
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 Uncharted2Tonemap(vec3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;

    return ((x*(A*x + C*B) + D*E) / 
            (x*(A*x + B) + D*F)) - E/F;
}