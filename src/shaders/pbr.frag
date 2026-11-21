#version 460

// 基于物理的 Cook-Torrance 光照（解析光源，无贴图）
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
    vec3  ambient;   float shininess;
    vec3  diffuse;   float metallic;
    vec3  specular;  float roughness;
};

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float rough) {
    float a  = rough * rough;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH * (a2 - 1.0) + 1.0);
    return a2 / max(PI * denom * denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float rough) {
    float r = rough + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float rough) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, rough) * GeometrySchlickGGX(NdotL, rough);
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(viewPos - fragPos);
    vec3 L = normalize(lightPos - fragPos);
    vec3 H = normalize(V + L);

    float rough = clamp(roughness, 0.05, 1.0);
    float metal = clamp(metallic, 0.0, 1.0);

    vec3  F0  = mix(vec3(0.04), diffuse, metal);
    float NDF = DistributionGGX(N, H, rough);
    float G   = GeometrySmith(N, V, L, rough);
    vec3  F   = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator   = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specularTerm = numerator / denominator;

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metal);

    float NdotL = max(dot(N, L), 0.0);
    vec3 radiance = lightColor;

    vec3 Lo = (kD * diffuse / PI + specularTerm) * radiance * NdotL;

    // 简易环境项，避免背光面全黑
    vec3 ambientTerm = ambient * diffuse * lightColor;

    vec3 color = ambientTerm + Lo;
    color *= 3.5;                            // 曝光补偿
    color = color / (color + vec3(1.0));    // Reinhard 色调映射
    outColor = vec4(color, 1.0);
}
