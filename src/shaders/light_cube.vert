#version 450 core

layout (location = 0) in vec3 vertexPosition;

layout(std140, binding = 0) uniform TransformUBO {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
};

void main()
{
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(vertexPosition, 1.0);
}