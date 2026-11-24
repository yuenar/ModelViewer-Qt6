#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

// 注意：qsb 会把 UBO 输出为普通结构体 uniform，实例名若留空会按 shader
// 内部 id 自动生成（如 _171），跨阶段可能撞名导致 GL 链接失败
// （Uniform _171 type mismatch）。这里显式命名，保证两阶段唯一且稳定。
layout(std140, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
    mat4 lightSpace;
    mat4 shadowBias;
} frameUbo;

layout(location = 3) in uvec4 inBoneIndices;
layout(location = 4) in vec4 inBoneWeights;

layout(std140, binding = 4) uniform SkinUBO {
    mat4 bones[128];
    mat4 objectMatrix;
    mat4 objectNormalMatrix;
    int boneCount;
    int skinEnabled;
    vec2 _skinPad;
} skinUbo;

void applySkin(inout vec3 position, inout vec3 normal) {
    if (skinUbo.skinEnabled == 0) {
        return;
    }

    int maxIndex = max(skinUbo.boneCount - 1, 0);
    vec4 accumulated = vec4(0.0);
    vec3 accumulatedNormal = vec3(0.0);
    float total = 0.0;
    for (int i = 0; i < 4; ++i) {
        float w = inBoneWeights[i];
        if (w <= 0.0) {
            continue;
        }
        int index = clamp(int(inBoneIndices[i]), 0, maxIndex);
        mat4 bone = skinUbo.bones[index];
        accumulated += w * (bone * vec4(position, 1.0));
        accumulatedNormal += w * (mat3(bone) * normal);
        total += w;
    }

    vec3 skinnedPosition = position;
    vec3 skinnedNormal = normal;
    if (total > 0.0) {
        skinnedPosition = accumulated.xyz / total;
        skinnedNormal = accumulatedNormal;
    }
    position = (skinUbo.objectMatrix * vec4(skinnedPosition, 1.0)).xyz;
    normal = mat3(skinUbo.objectNormalMatrix) * skinnedNormal;
}

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec4 vLightClip;
layout(location = 3) out vec2 vTexCoord;

void main() {
    vec3 position = inPosition;
    vec3 normal = inNormal;
    applySkin(position, normal);

    vec4 worldPos = frameUbo.model * vec4(position, 1.0);
    fragPos       = worldPos.xyz;
    fragNormal    = mat3(frameUbo.normalMatrix) * normal;
    vLightClip    = frameUbo.shadowBias * frameUbo.lightSpace * worldPos;
    vTexCoord     = inTexCoord;
    gl_Position   = frameUbo.projection * frameUbo.view * worldPos;
}
