#version 460

layout(location = 0) in float vY;
layout(location = 0) out vec4 outColor;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
};

void main() {
    vec3 topColor = vec3(0.2, 0.3, 0.5);
    vec3 bottomColor = vec3(0.8, 0.9, 1.0);
    float t = (vY + 1.0) * 0.5;
    outColor = vec4(mix(bottomColor, topColor, t), 1.0);
}
