#version 460

layout(location = 0) out vec4 outColor;

void main() {
    // 剖面封盖使用中性色，便于与实体表面区分
    outColor = vec4(0.82, 0.84, 0.88, 1.0);
}
