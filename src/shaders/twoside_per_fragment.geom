#version 450 core

layout(triangles) in;
layout(triangle_strip, max_vertices=6) out;

layout(location = 0) in vec3 v_position[];
layout(location = 1) in vec3 v_normal[];
layout(location = 2) in vec2 v_texCoord2d[];
layout(location = 3) in vec3 v_tangent[];
layout(location = 4) in vec3 v_bitangent[];

layout(location = 0) out vec3 g_normal;
layout(location = 1) out vec3 g_position;
layout(location = 2) out vec2 g_texCoord2d;
layout(location = 3) out vec3 g_tangent;
layout(location = 4) out vec3 g_bitangent;

layout(location = 5) out vec3 g_edgeDistance;

layout(std140, set = 0, binding = 0) uniform TwoSideGeomUBO {
    mat4 viewportMatrix;
    int displayMode;
    float _p0;
    float _p1;
    float _p2;
};

layout(location = 6) in vec3 v_FragPos[];
layout(location = 7) in vec3 v_Normal[];
layout(location = 8) in vec2 v_TexCoords[];
layout(location = 9) in vec4 v_FragPosLightSpace[];
layout(location = 10) in vec3 v_cameraPos[];
layout(location = 11) in vec3 v_lightPos[];

layout(location = 6) out vec3 g_FragPos;
layout(location = 7) out vec3 g_Normal;
layout(location = 8) out vec2 g_TexCoords;
layout(location = 9) out vec4 g_FragPosLightSpace;
layout(location = 10) out vec3 g_cameraPos;
layout(location = 11) out vec3 g_lightPos;

layout(location = 12) in vec3  v_reflectionPosition[];
layout(location = 12) out vec3 g_reflectionPosition;

layout(location = 13) in vec3  v_reflectionNormal[];
layout(location = 13) out vec3 g_reflectionNormal;

layout(location = 14) in vec3 v_tangentLightPos[];
layout(location = 15) in vec3 v_tangentViewPos[];
layout(location = 16) in vec3 v_tangentFragPos[];
layout(location = 14) out vec3 g_tangentLightPos;
layout(location = 15) out vec3 g_tangentViewPos;
layout(location = 16) out vec3 g_tangentFragPos;

layout(location = 17) in float v_clipDistX[];
layout(location = 18) in float v_clipDistY[];
layout(location = 19) in float v_clipDistZ[];
layout(location = 20) in float v_clipDist[];

layout(location = 17) out float g_clipDistX;
layout(location = 18) out float g_clipDistY;
layout(location = 19) out float g_clipDistZ;
layout(location = 20) out float g_clipDist;

void main()
{
    // initialize to remove warning
    g_edgeDistance = vec3(0);
    g_FragPos = vec3(0);
    g_Normal = vec3(0);
    g_TexCoords = vec2(0);
    g_FragPosLightSpace = vec4(0);
    g_cameraPos = vec3(0);
    g_lightPos = vec3(0);
    g_reflectionPosition = vec3(0);
    g_reflectionNormal = vec3(0);
    g_tangentLightPos = vec3(0);
    g_tangentViewPos = vec3(0);
    g_tangentFragPos = vec3(0);
    g_tangent = vec3(0);
    g_bitangent = vec3(0);
    // end initialization

    if(displayMode == 2) // WireShaded
    {
        // Transform each vertex into viewport space
        vec3 p0 = vec3(viewportMatrix * (gl_in[0].gl_Position /
        gl_in[0].gl_Position.w));
        vec3 p1 = vec3(viewportMatrix * (gl_in[1].gl_Position /
        gl_in[1].gl_Position.w));
        vec3 p2 = vec3(viewportMatrix * (gl_in[2].gl_Position /
        gl_in[2].gl_Position.w));
        // Find the altitudes (ha, hb and hc)
        float a = length(p1 - p2);
        float b = length(p2 - p0);
        float c = length(p1 - p0);
        float alpha = acos( (b*b + c*c - a*a) / (2.0*b*c) );
        float beta = acos( (a*a + c*c - b*b) / (2.0*a*c) );
        float ha = abs( c * sin( beta ) );
        float hb = abs( c * sin( alpha ) );
        float hc = abs( b * sin( alpha ) );

        // Send the triangle along with the edge distances
        g_edgeDistance = vec3( ha, 0, 0 );
        g_normal = v_normal[0];
        g_texCoord2d = v_texCoord2d[0];
        g_position = v_position[0];
        gl_Position = gl_in[0].gl_Position;
        g_clipDistX = v_clipDistX[0];
        g_clipDistY = v_clipDistY[0];
        g_clipDistZ = v_clipDistZ[0];
        g_clipDist =  v_clipDist[0];
        EmitVertex();

        g_edgeDistance = vec3( 0, hb, 0 );
        g_normal = v_normal[1];
        g_texCoord2d = v_texCoord2d[1];
        g_position = v_position[1];
        gl_Position = gl_in[1].gl_Position;
        g_clipDistX = v_clipDistX[1];
        g_clipDistY = v_clipDistY[1];
        g_clipDistZ = v_clipDistZ[1];
        g_clipDist =  v_clipDist[1];
        EmitVertex();

        g_edgeDistance = vec3( 0, 0, hc );
        g_normal = v_normal[2];
        g_texCoord2d = v_texCoord2d[2];
        g_position = v_position[2];
        gl_Position = gl_in[2].gl_Position;
        g_clipDistX = v_clipDistX[2];
        g_clipDistY = v_clipDistY[2];
        g_clipDistZ = v_clipDistZ[2];
        g_clipDist =  v_clipDist[2];
        EmitVertex();

        EndPrimitive();
    }
    else
    {
        for(int i=0; i<3; i++)
        {
            g_normal = v_normal[i];
            g_texCoord2d = v_texCoord2d[i];
            g_position = v_position[i];
            gl_Position = gl_in[i].gl_Position;

            g_clipDistX = v_clipDistX[i];
            g_clipDistY = v_clipDistY[i];
            g_clipDistZ = v_clipDistZ[i];
            g_clipDist =  v_clipDist[i];

            // Shadow mapping
            g_FragPos = v_FragPos[i];
            g_Normal = v_Normal[i];
            g_TexCoords = v_TexCoords[i];
            g_FragPosLightSpace = v_FragPosLightSpace[i];
            g_cameraPos = v_cameraPos[i];
            g_lightPos = v_lightPos[i];

            // Cube environment mapping
            g_reflectionPosition = v_reflectionPosition[i];
            g_reflectionNormal = v_reflectionNormal[i];

            g_tangentLightPos = v_tangentLightPos[i];
            g_tangentViewPos = v_tangentViewPos[i];
            g_tangentFragPos = v_tangentFragPos[i];
            g_tangent = v_tangent[i];
            g_bitangent = v_bitangent[i];

            EmitVertex();
        }
        EndPrimitive();
    }
}
