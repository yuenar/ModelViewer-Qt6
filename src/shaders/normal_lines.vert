#version 460

// 法线线段：每个顶点携带「起点」与「方向」，由着色器按 length 伸缩
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inDirection;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
};

layout(std140, binding = 2) uniform NormalUBO {
    vec3  color;
    float length;
};

void main() {
    vec4 worldPos = model * vec4(inPosition + inDirection * length, 1.0);
    gl_Position   = projection * view * worldPos;
}
