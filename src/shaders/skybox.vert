#version 460

layout(location = 0) in vec3 inPosition;

layout(std140, binding = 0) uniform SkyboxUBO {
    mat4 projection;
    mat4 view;          // 去掉平移的视图矩阵
};

layout(location = 0) out vec3 vDirection;

void main() {
    vDirection = inPosition;
    vec4 pos = projection * view * vec4(inPosition, 1.0);
    gl_Position = pos.xyww;   // 深度固定在远平面
}
