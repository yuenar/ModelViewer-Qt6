#version 450 core

layout(location = 0) in vec3 vertexPosition;

layout(location = 0) out vec3 texCoords;
layout(location = 1) out vec3 worldPos;

uniform mat4 projectionMatrix;
uniform mat4 viewMatrix;

void main()
{
    texCoords = vertexPosition;
    worldPos = vertexPosition;
    vec4 pos = projectionMatrix * viewMatrix * vec4(vertexPosition, 1.0);
    gl_Position = pos.xyww;
}