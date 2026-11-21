#version 460

layout(location = 0) in vec3 inCenter;
layout(location = 1) in vec2 inCorner;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inColor;
layout(location = 4) in float inScale;

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

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec3 vColor;

void main() {
    // 从视图矩阵提取相机的右/上方向，构造公告板
    vec3 right = vec3(view[0][0], view[1][0], view[2][0]);
    vec3 up    = vec3(view[0][1], view[1][1], view[2][1]);
    vec3 world = inCenter + (right * inCorner.x + up * inCorner.y) * inScale;

    vUV = inUV;
    vColor = inColor;
    gl_Position = projection * view * vec4(world, 1.0);
}
