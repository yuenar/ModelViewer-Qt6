#version 460

layout(location = 0) out vec4 outColor;

layout(std140, binding = 2) uniform NormalUBO {
    vec3  color;
    float length;
};

void main() {
    outColor = vec4(color, 1.0);
}
