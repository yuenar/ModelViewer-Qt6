#version 440

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
};

layout(std140, binding = 2) uniform NormalsUBO {
    float normalLength;
};

void main() {
    vec4 worldPos = model * vec4(inPosition, 1.0);
    vec3 normal = normalize(mat3(normalMatrix) * inNormal);
    
    // 根据顶点索引决定输出原始顶点还是法线终点
    if (gl_VertexIndex % 2 == 0) {
        // 偶数索引：输出原始顶点
        gl_Position = projection * view * worldPos;
    } else {
        // 奇数索引：输出法线终点
        vec3 normalEnd = worldPos.xyz + normal * normalLength;
        gl_Position = projection * view * vec4(normalEnd, 1.0);
    }
}

