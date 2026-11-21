#version 460

layout(std140, binding = 1) uniform SelectionUBO {
    vec4 color;
};

layout(location = 0) out vec4 fragColor;

void main() {
    fragColor = color;
}
