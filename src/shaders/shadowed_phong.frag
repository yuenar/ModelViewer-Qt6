#version 460

layout(location = 0) in  vec3 fragPos;
layout(location = 1) in  vec3 fragNormal;
layout(location = 2) in  vec4 vLightClip;
layout(location = 3) in  vec2 vTexCoord;
layout(location = 0) out vec4 outColor;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model; mat4 view; mat4 projection; mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
    mat4 lightSpace;
    mat4 shadowBias;
};

layout(std140, binding = 1) uniform MaterialUBO {
    vec3  ambient;   float shininess;
    vec3  diffuse;   float metallic;
    vec3  specular;  float roughness;
};

// 深度比较采样器（与 QRhiSampler::setTextureCompareOp 配合）
layout(binding = 2) uniform sampler2DShadow shadowMap;
layout(binding = 3) uniform sampler2D diffuseMap;

float sampleShadow(vec4 lightClip) {
    vec3 v = lightClip.xyz / lightClip.w;
    if (v.x < 0.0 || v.x > 1.0 || v.y < 0.0 || v.y > 1.0 || v.z > 1.0) {
        return 1.0;
    }

    const float bias = 0.002;
    vec2 texel = 1.0 / vec2(textureSize(shadowMap, 0));
    float sum = 0.0;
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            sum += texture(shadowMap, vec3(v.xy + vec2(x, y) * texel, v.z - bias));
        }
    }
    return sum / 9.0;
}

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(lightPos - fragPos);
    vec3 V = normalize(viewPos - fragPos);
    vec3 R = reflect(-L, N);

    float shadow = sampleShadow(vLightClip);

    vec3 a = ambient * lightColor;
    vec3 d = diffuse * lightColor * max(dot(N, L), 0.0) * shadow;
    vec3 s = specular * lightColor * pow(max(dot(R, V), 0.0), max(shininess, 1.0)) * shadow;

    outColor = vec4((a + d + s) * texture(diffuseMap, vTexCoord).rgb, 1.0);
}
