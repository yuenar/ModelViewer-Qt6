#version 450 core
layout (triangles) in;
layout (triangle_strip, max_vertices=256) out;

// MVP 从顶点 varying (in mat4 MVP[]) 移到 UBO
// 原因：SPIRV-Cross 无法将矩阵类型的 geometry shader input varying 转译为 HLSL
layout(std140, set = 0, binding = 0) uniform SubdivUBO {
    int   sub_divisions;
    float _p0;
    float _p1;
    float _p2;
    mat4  MVP;          // ← 移到此处（原先为 in mat4 MVP[]）
};

void main()
{
    // 顶点着色器只需 pass-through 原始位置，几何着色器在此处应用 MVP
    vec4 v0 = gl_in[0].gl_Position;
    vec4 v1 = gl_in[1].gl_Position;
    vec4 v2 = gl_in[2].gl_Position;

    float dx = abs(v0.x - v2.x) / float(sub_divisions);
    float dz = abs(v0.z - v1.z) / float(sub_divisions);

    float x = v0.x;
    float z = v0.z;

    for (int j = 0; j < sub_divisions * sub_divisions; j++) {
        gl_Position = MVP * vec4(x,      0.0, z,      1.0); EmitVertex();
        gl_Position = MVP * vec4(x,      0.0, z + dz, 1.0); EmitVertex();
        gl_Position = MVP * vec4(x + dx, 0.0, z,      1.0); EmitVertex();
        gl_Position = MVP * vec4(x + dx, 0.0, z + dz, 1.0); EmitVertex();
        EndPrimitive();

        x += dx;
        if ((j + 1) % sub_divisions == 0) {
            x  = v0.x;
            z += dz;
        }
    }
}
