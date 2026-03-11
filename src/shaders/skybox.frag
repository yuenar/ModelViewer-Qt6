#version 450 core

layout(location = 0) out vec4 fragColor;

layout(location = 0) in vec3 texCoords;

layout(std140, binding = 0) uniform SkyboxUBO {
    bool hdrToneMapping;
    bool gammaCorrection;
    float screenGamma;
    float _p0;
};

layout(binding = 1) uniform samplerCube skybox;

void main()
{
    fragColor = texture(skybox, texCoords);

    // HDR tonemapping
    if(hdrToneMapping)
        fragColor = fragColor / (fragColor + vec4(1.0));
    // gamma correct
    if(gammaCorrection)
        fragColor = pow(fragColor, vec4(1.0/screenGamma));
}