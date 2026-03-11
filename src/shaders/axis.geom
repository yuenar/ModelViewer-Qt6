#version 450 core

layout(lines) in;
layout(line_strip, max_vertices=2) out;

layout(location = 0) in vec3 v_position[];
layout(location = 1) in vec3 v_normal[];
layout(location = 2) in vec2 v_texCoord2d[];

layout(location = 0) out vec3 g_normal;
layout(location = 1) out vec3 g_position;
layout(location = 2) out vec2 g_texCoord2d;

layout(location = 3) out vec3 g_edgeDistance;

layout(std140, binding = 0) uniform AxisGeomUBO {
    mat4 viewportMatrix;
};

layout(location = 4) in float clipDistX[];
layout(location = 5) in float clipDistY[];
layout(location = 6) in float clipDistZ[];
layout(location = 7) in float clipDist[];

layout(location = 4) out float g_clipDistX;
layout(location = 5) out float g_clipDistY;
layout(location = 6) out float g_clipDistZ;
layout(location = 7) out float g_clipDist;

void main()
{	
    for(int i=0; i<2; i++)
    {
        g_normal = v_normal[i];
        g_texCoord2d = v_texCoord2d[i];
        g_position = v_position[i];
        gl_Position = gl_in[i].gl_Position;

        g_clipDistX = clipDistX[i];
        g_clipDistY = clipDistY[i];
        g_clipDistZ = clipDistZ[i];
        g_clipDist = clipDist[i];

        EmitVertex();
    }
    EndPrimitive();
}  
