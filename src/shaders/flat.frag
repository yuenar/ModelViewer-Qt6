#version 460

layout(location = 0) in  vec3 fragPos;
layout(location = 1) in  vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(binding = 3) uniform sampler2D diffuseMap;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model; mat4 view; mat4 projection; mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
};

layout(std140, binding = 1) uniform MaterialUBO {
    vec3  ambient;   float shininess;
    vec3  diffuse;   float metallic;
    vec3  specular;  float roughness;
};

void main() {
    // 用屏幕空间导数求面法线，得到逐面（平面）着色
    vec3 N = normalize(cross(dFdx(fragPos), dFdy(fragPos)));
    vec3 V = normalize(viewPos - fragPos);
    if (dot(N, V) < 0.0) {
        N = -N;
    }
    vec3 L = normalize(lightPos - fragPos);
    float diff = max(dot(N, L), 0.0);

    vec3 color = ambient * lightColor + diffuse * lightColor * diff;
    outColor = vec4(color * texture(diffuseMap, vTexCoord).rgb, 1.0);
}
