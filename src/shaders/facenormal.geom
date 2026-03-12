#version 450 core
layout(triangles) in;
layout(line_strip, max_vertices=8) out;

// MVP 从 in mat4 MVP[] 移到 UBO，原因同 subdiv.geom
// 同时修复原代码 bug：for 循环后 i==3，MVP[3] 越界（只有索引 0/1/2）
layout(std140, set = 0, binding = 0) uniform FaceNormalUBO {
    float normal_length;
    float _p0;
    float _p1;
    float _p2;
    mat4  MVP;          // ← 移到此处（原先为 in mat4 MVP[]，且有越界 bug）
};

layout(location = 0) in Vertex
{
    layout(location = 0) vec4 normal;
    layout(location = 1) vec4 color;
} vertex[];

layout(location = 0) out vec4 v_color;

void main()
{
    // 3 条顶点法线线段
    for (int i = 0; i < gl_in.length(); i++)
    {
        vec3 P = gl_in[i].gl_Position.xyz;
        vec3 N = vertex[i].normal.xyz;

        gl_Position = MVP * vec4(P, 1.0);
        v_color     = vertex[i].color;
        EmitVertex();

        gl_Position = MVP * vec4(P + N * normal_length, 1.0);
        v_color     = vertex[i].color;
        EmitVertex();

        EndPrimitive();
    }

    // 面法线线段（取三角形中心）
    vec3 P0 = gl_in[0].gl_Position.xyz;
    vec3 P1 = gl_in[1].gl_Position.xyz;
    vec3 P2 = gl_in[2].gl_Position.xyz;

    vec3 V0 = P0 - P1;
    vec3 V1 = P2 - P1;
    vec3 N  = normalize(cross(V1, V0));
    vec3 P  = (P0 + P1 + P2) / 3.0;

    // 原代码用 MVP[i]（此时 i==3，越界）→ 改为 MVP（UBO 中的单一矩阵）
    gl_Position = MVP * vec4(P, 1.0);
    v_color     = vec4(1.0, 0.0, 0.0, 1.0);
    EmitVertex();

    gl_Position = MVP * vec4(P + N * normal_length, 1.0);
    v_color     = vec4(1.0, 0.0, 0.0, 1.0);
    EmitVertex();

    EndPrimitive();
}
