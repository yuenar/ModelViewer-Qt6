#version 460

layout(location = 0) in vec3 inPosition;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
    mat4 lightSpace;
    mat4 shadowBias;
};

void main() {
    gl_Position = projection * view * model * vec4(inPosition, 1.0);
}
