#version 450 core

layout(location = 0) in vec3 vertexPosition;

layout(std140, binding = 0) uniform TransformUBO {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 projectionMatrix;
};

layout(std140, binding = 1) uniform ClipPlanesUBO {
    vec4 clipPlaneX;
    vec4 clipPlaneY;
    vec4 clipPlaneZ;
    vec4 clipPlane;
};

layout(location = 0) out float v_clipDistX;
layout(location = 1) out float v_clipDistY;
layout(location = 2) out float v_clipDistZ;
layout(location = 3) out float v_clipDist;

void main()
{
    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(vertexPosition, 1);

    v_clipDistX = dot(clipPlaneX, viewMatrix * modelMatrix * vec4(vertexPosition, 1));
    v_clipDistY = dot(clipPlaneY, viewMatrix * modelMatrix* vec4(vertexPosition, 1));
    v_clipDistZ = dot(clipPlaneZ, viewMatrix * modelMatrix* vec4(vertexPosition, 1));
    v_clipDist =  dot(clipPlane, viewMatrix * modelMatrix* vec4(vertexPosition, 1));

    gl_ClipDistance[0] = v_clipDistX;
    gl_ClipDistance[1] = v_clipDistY;
    gl_ClipDistance[2] = v_clipDistZ;
    gl_ClipDistance[3] = v_clipDist;
}