#version 450 core

layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform LightCubeUBO {
    vec3 lightColor;
    float _p0;
};

void main()
{
    fragColor = vec4(lightColor, 1.0f);
}