#version 450 core

layout (triangles) in;
layout (line_strip, max_vertices = 6) out;

layout(location = 0) in VS_OUT {
    layout(location = 0) vec3 normal;
} gs_in[];

const float MAGNITUDE = 0.05;

layout(location = 0) out vec3 g_normal;
layout(location = 1) out vec3 g_position;
layout(location = 2) out vec2 g_texCoord2d;

layout(location = 3) in float clipDistX[];
layout(location = 4) in float clipDistY[];
layout(location = 5) in float clipDistZ[];
layout(location = 6) in float clipDist[];

layout(location = 3) out float g_clipDistX;
layout(location = 4) out float g_clipDistY;
layout(location = 5) out float g_clipDistZ;
layout(location = 6) out float g_clipDist;

void GenerateLine(int index)
{
    gl_Position = gl_in[index].gl_Position;
    g_clipDistX = clipDistX[index];
    g_clipDistY = clipDistY[index];
    g_clipDistZ = clipDistZ[index];
    g_clipDist = clipDist[index];

    gl_ClipDistance[0] = g_clipDistX;
    gl_ClipDistance[1] = g_clipDistY;
    gl_ClipDistance[2] = g_clipDistZ;
    gl_ClipDistance[3] = g_clipDist;
    EmitVertex();

    gl_Position = gl_in[index].gl_Position + vec4(gs_in[index].normal, 0.0) * MAGNITUDE;
    g_clipDistX = clipDistX[index];
    g_clipDistY = clipDistY[index];
    g_clipDistZ = clipDistZ[index];
    g_clipDist = clipDist[index];

    gl_ClipDistance[0] = g_clipDistX;
    gl_ClipDistance[1] = g_clipDistY;
    gl_ClipDistance[2] = g_clipDistZ;
    gl_ClipDistance[3] = g_clipDist;
    EmitVertex();

    EndPrimitive();
}

void main()
{
    GenerateLine(0); // first vertex normal
    GenerateLine(1); // second vertex normal
    GenerateLine(2); // third vertex normal
}