#version 460

layout(location = 0) in vec2 inPos;

layout(location = 0) out vec2 v_uv;

void main() {
    v_uv        = inPos * 0.5 + 0.5;
    gl_Position = vec4(inPos, 1.0, 1.0);
}
