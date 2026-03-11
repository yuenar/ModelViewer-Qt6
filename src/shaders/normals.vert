#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
};

out VS_OUT {
    layout(location = 0) vec3 normal;
    layout(location = 1) vec3 position;
} vs_out;

void main() {
    vs_out.position = (model * vec4(inPosition, 1.0)).xyz;
    vs_out.normal = mat3(normalMatrix) * inNormal;
    gl_Position = projection * view * model * vec4(inPosition, 1.0);
}
