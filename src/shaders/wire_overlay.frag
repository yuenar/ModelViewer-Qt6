#version 460

// Wire-Shaded 叠加线框：深色线条，保证在浅色实体表面上可辨识
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(0.05, 0.05, 0.08, 1.0);
}
