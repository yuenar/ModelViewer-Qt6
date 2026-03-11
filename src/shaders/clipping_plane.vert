#version 450 core

layout (location = 0) in vec3 vertexPosition;
layout(location = 1) in vec2 texCoord2d;

layout(std140, binding = 0) uniform TransformUBO {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
};

layout(location = 0) out vec2 texCoord;

void main()
{
    texCoord = texCoord2d;
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(vertexPosition, 1.0);
}