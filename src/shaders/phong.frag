#version 460

layout(location = 0) in  vec3 fragPos;
layout(location = 1) in  vec3 fragNormal;
layout(location = 0) out vec4 outColor;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model; mat4 view; mat4 projection; mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
};

layout(std140, binding = 1) uniform MaterialUBO {
    vec3  ambient;  float shininess;
    vec3  diffuse;  float _mp0;
    vec3  specular; float _mp1;
};

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(lightPos - fragPos);
    vec3 V = normalize(viewPos  - fragPos);
    vec3 R = reflect(-L, N);

    vec3 a = ambient  * lightColor;
    vec3 d = diffuse  * lightColor * max(dot(N, L), 0.0);
    vec3 s = specular * lightColor * pow(max(dot(R, V), 0.0), shininess);

    outColor = vec4(a + d + s, 1.0);
}
