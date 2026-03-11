#version 450 core

layout(location = 0) in vec3 vertexPosition;

layout(std140, binding = 0) uniform SkyboxTransformUBO {
    mat4 projectionMatrix;
    mat4 viewMatrix;
};

layout(location = 0) out vec3 texCoords;
layout(location = 1) out vec3 worldPos;

void main()
{
    texCoords = vertexPosition;
    worldPos = vertexPosition;
    vec4 pos = projectionMatrix * viewMatrix * vec4(vertexPosition, 1.0);
    gl_Position = pos.xyww;
}