#version 460

layout(location = 0) in vec2 inPosition;

layout(location = 0) out float vY;

void main() {
    vY = inPosition.y;
    gl_Position = vec4(inPosition, 0.0, 1.0);
}
