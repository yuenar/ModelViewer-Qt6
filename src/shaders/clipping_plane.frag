#version 450 core

layout(location = 0) in vec2 texCoord;

layout(std140, binding = 0) uniform ClippingPlaneUBO {
    bool selected;
    float _p0;
    float _p1;
    float _p2;
    vec3 planeColor;
};

layout(binding = 1) uniform sampler2D hatchMap;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = vec4(planeColor, 1.0f);
    fragColor = mix(fragColor, texture(hatchMap, texCoord), 0.25);

    if(selected)
    {
        fragColor = mix(fragColor, vec4(1.0f, .65f, 0.0f, 1.0f), 0.5f);
    }
}