#version 450 core

layout(location = 0) in vec3 g_position;
layout(location = 1) in vec3 g_normal;
layout(location = 2) in vec2 g_texCoord2d;
layout(location = 3) in vec3 g_tangent;
layout(location = 4) in vec3 g_bitangent;
noperspective layout(location = 5) in vec3 g_edgeDistance;
layout(location = 6) in vec3 g_reflectionPosition;
layout(location = 7) in vec3 g_reflectionNormal;
layout(location = 8) in vec3 g_tangentLightPos;
layout(location = 9) in vec3 g_tangentViewPos;
layout(location = 10) in vec3 g_tangentFragPos;

in GS_OUT_SHADOW {
    layout(location = 11) vec3 FragPos;
    layout(location = 12) vec3 Normal;
    layout(location = 13) vec2 TexCoords;
    layout(location = 14) vec4 FragPosLightSpace;
    layout(location = 15) vec3 cameraPos;
    layout(location = 16) vec3 lightPos;
} fs_in_shadow;

layout(std140, binding = 0) uniform TwoSideFragUBO {
    float opacity;
    bool texEnabled;
    float _p0;
    float _p1;
};

layout(std140, binding = 1) uniform MaterialUBO {
    vec3 emission;
    float _mp0;
    vec3 ambient;
    float _mp1;
    vec3 diffuse;
    float _mp2;
    vec3 specular;
    float shininess;
    bool metallic;
    float _mp3;
    float _mp4;
    float _mp5;
};

layout(std140, binding = 2) uniform LightingUBO {
    vec3 lightAmbient;
    float _lp0;
    vec3 lightDiffuse;
    float _lp1;
    vec3 lightSpecular;
    float _lp2;
    vec3 lightPosition;
    float _lp3;
    vec3 modelAmbient;
    float _lp4;
    vec3 cameraPos;
    float _lp5;
};

layout(std140, binding = 3) uniform DisplayUBO {
    int displayMode;
    int renderingMode;
    bool selected;
    bool sectionActive;
    bool floorRendering;
    bool lockLightAndCamera;
    bool hdrToneMapping;
    bool gammaCorrection;
    float screenGamma;
    float _dp0;
    float _dp1;
    float _dp2;
};

layout(binding = 4) uniform sampler2D texUnit;

layout( location = 0 ) out vec4 fragColor;

void main()
{
    vec4 v_color = vec4(1.0);
    
    if(renderingMode == 0)
    {
        v_color = vec4(diffuse, opacity);
    }
    
    if(gl_FrontFacing)
    {
        fragColor = v_color;
    }
    else
    {
        if(sectionActive)
            fragColor = v_color + 0.15;
        else
            fragColor = v_color;
    }
    
    if(texEnabled)
        fragColor = fragColor * texture(texUnit, g_texCoord2d);
    
    if(selected)
    {
        vec3 objectColor = vec3(1.0, 0.6392156862745098, 0.396078431372549);
        fragColor = vec4(objectColor, opacity);
    }
    
    if(hdrToneMapping)
        fragColor = fragColor / (fragColor + vec4(1.0));
    
    if(gammaCorrection)
        fragColor = pow(fragColor, vec4(1.0/screenGamma));
}
