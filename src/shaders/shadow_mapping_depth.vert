#version 450 core
layout (location = 0) in vec3 aPos;

layout(std140, binding = 0) uniform ShadowUBO {
    mat4 lightSpaceMatrix;
    mat4 model;
};

void main()
{
    gl_Position = lightSpaceMatrix * model * vec4(aPos, 1.0);
}