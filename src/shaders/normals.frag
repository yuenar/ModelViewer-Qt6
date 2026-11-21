#version 460

layout(location = 0) in  vec3 vNormalColor;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(vNormalColor, 1.0);
}
