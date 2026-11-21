#version 460

layout(location = 0) in vec3 inPosition;

layout(std140, binding = 0) uniform ShadowUBO {
    mat4 lightSpace;
    mat4 shadowBias;
};

void main() {
    gl_Position = lightSpace * vec4(inPosition, 1.0);
}
