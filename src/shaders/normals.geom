#version 460

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

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

layout(location = 0) in VS_OUT {
    layout(location = 0) vec3 normal;
    layout(location = 1) vec3 position;
} gs_in[];

layout(location = 0) out vec3 fragColor;

void main() {
    for(int i = 0; i < 3; i++) {
        // 绘制原始顶点
        gl_Position = projection * view * vec4(gs_in[i].position, 1.0);
        fragColor = vec3(1.0, 1.0, 1.0);
        EmitVertex();
        
        // 绘制法线端点
        vec3 normalEnd = gs_in[i].position + normalize(gs_in[i].normal) * normalLength;
        gl_Position = projection * view * vec4(normalEnd, 1.0);
        fragColor = vec3(1.0, 0.0, 0.0);
        EmitVertex();
        
        EndPrimitive();
    }
}
