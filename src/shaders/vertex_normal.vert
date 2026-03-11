#version 450 core

layout (location = 0) in vec3 vertexPosition;
layout (location = 1) in vec3 vertexNormal;

out VS_OUT {
    layout(location = 0) vec3 normal;
} vs_out;

layout(std140, binding = 0) uniform TransformUBO {
    mat4 modelViewMatrix;
    mat4 projectionMatrix;
};

layout(std140, binding = 1) uniform ClipPlanesUBO {
    vec4 clipPlaneX;
    vec4 clipPlaneY;
    vec4 clipPlaneZ;
    vec4 clipPlane;
};

layout(location = 1) out float clipDistX;
layout(location = 2) out float clipDistY;
layout(location = 3) out float clipDistZ;
layout(location = 4) out float clipDist;

void main()
{
    mat3 normalMatrix = mat3(transpose(inverse(modelViewMatrix)));
    vs_out.normal = normalize(vec3(projectionMatrix * vec4(normalMatrix * vertexNormal, 0.0)));
    gl_Position = projectionMatrix * modelViewMatrix * vec4(vertexPosition, 1.0);

    clipDistX = dot(clipPlaneX, modelViewMatrix* vec4(vertexPosition, 1));
    clipDistY = dot(clipPlaneY, modelViewMatrix* vec4(vertexPosition, 1));
    clipDistZ = dot(clipPlaneZ, modelViewMatrix* vec4(vertexPosition, 1));
    clipDist = dot(clipPlane, modelViewMatrix* vec4(vertexPosition, 1));
}