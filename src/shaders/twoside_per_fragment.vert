#version 450 core

layout(location = 0) in vec3 vertexPosition;
layout(location = 1) in vec3 vertexNormal;
layout(location = 2) in vec2 texCoord2d;
layout(location = 3) in vec3 vertexTangent;
layout(location = 4) in vec3 vertexBitangent;

layout(std140, binding = 0) uniform TransformUBO {
    mat4 modelMatrix;
    mat4 viewMatrix;
    mat4 modelViewMatrix;
    mat4 projectionMatrix;
};

layout(std140, binding = 1) uniform ClipPlanesUBO {
    vec4 clipPlaneX;
    vec4 clipPlaneY;
    vec4 clipPlaneZ;
    vec4 clipPlane;
};

layout(std140, binding = 2) uniform LightingUBO {
    mat3 normalMatrix;
    float _p0;
    mat4 lightSpaceMatrix;
    vec3 cameraPos;
    float _p1;
    vec3 lightPos;
    float _p2;
};

layout(location = 0) out float v_clipDistX;
layout(location = 1) out float v_clipDistY;
layout(location = 2) out float v_clipDistZ;
layout(location = 3) out float v_clipDist;

layout(location = 4) out vec3 v_normal;
layout(location = 5) out vec3 v_position;
layout(location = 6) out vec2 v_texCoord2d;
layout(location = 7) out vec3 v_tangent;
layout(location = 8) out vec3 v_bitangent;
layout(location = 9) out vec3 v_tangentLightPos;
layout(location = 10) out vec3 v_tangentViewPos;
layout(location = 11) out vec3 v_tangentFragPos;

layout(location = 12) out vec3 v_reflectionPosition;
layout(location = 13) out vec3 v_reflectionNormal;

out VS_OUT_SHADOW {
    layout(location = 14) vec3 FragPos;
    layout(location = 15) vec3 Normal;
    layout(location = 16) vec2 TexCoords;
    layout(location = 17) vec4 FragPosLightSpace;
    layout(location = 18) vec3 cameraPos;
    layout(location = 19) vec3 lightPos;
} vs_out_shadow;

void main()
{
    v_normal     = normalize(normalMatrix * vertexNormal);                       // normal vector
    //v_normal = mat3(transpose(inverse(modelMatrix))) * vertexNormal;
    v_position   = vec3(modelMatrix * vec4(vertexPosition, 1));              // vertex pos in eye coords
    v_texCoord2d = texCoord2d;
    v_tangent = normalize(normalMatrix * vertexTangent);
    v_bitangent = normalize(normalMatrix * vertexBitangent);

    gl_Position = projectionMatrix * viewMatrix * modelMatrix * vec4(vertexPosition, 1);

    v_clipDistX = dot(clipPlaneX, modelViewMatrix* vec4(vertexPosition, 1));
    v_clipDistY = dot(clipPlaneY, modelViewMatrix* vec4(vertexPosition, 1));
    v_clipDistZ = dot(clipPlaneZ, modelViewMatrix* vec4(vertexPosition, 1));
    v_clipDist = dot(clipPlane, modelViewMatrix* vec4(vertexPosition, 1));

    // Shadow mapping
    vs_out_shadow.FragPos = vec3(modelMatrix * vec4(vertexPosition, 1.0));
    vs_out_shadow.Normal = normalize(mat3(transpose(inverse(modelMatrix))) * vertexNormal);
    vs_out_shadow.TexCoords = v_texCoord2d;
    vs_out_shadow.FragPosLightSpace = lightSpaceMatrix * vec4(vs_out_shadow.FragPos, 1.0);
    vs_out_shadow.cameraPos = cameraPos;
    vs_out_shadow.lightPos = lightPos;

    // Cube environment mapping
    v_reflectionPosition = vec3(modelMatrix * vec4(vertexPosition, 1.0));
    v_reflectionNormal = normalize(mat3(transpose(inverse(modelMatrix))) * vertexNormal);

    // Depth mapping
    vec3 T = normalize((mat3(modelViewMatrix)) * vertexTangent);
    //vec3 B = normalize((mat3(modelViewMatrix)) * vertexBitangent);
    vec3 N = normalize((mat3(modelViewMatrix)) * vertexNormal);
    vec3 B = cross(N, T);
    if (dot(cross(N, T), B) < 0.0f)
    {
        T = T * -1.0f;
    }
    mat3 TBN = transpose(mat3(T, B, N));

    v_tangentLightPos = TBN * lightPos;
    v_tangentViewPos  = TBN * cameraPos;
    v_tangentFragPos  = TBN * v_position;

    // Moved this to geometry shader
    /*
    gl_ClipDistance[0] = clipDistX;
    gl_ClipDistance[1] = clipDistY;
    gl_ClipDistance[2] = clipDistZ;
    gl_ClipDistance[3] = clipDist;
    */
}