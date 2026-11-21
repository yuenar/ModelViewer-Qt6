#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
};

layout(location = 3) in uvec4 inBoneIndices;
layout(location = 4) in vec4 inBoneWeights;

layout(std140, binding = 4) uniform SkinUBO {
    mat4 bones[128];
    mat4 objectMatrix;
    mat4 objectNormalMatrix;
    int boneCount;
    int skinEnabled;
    vec2 _skinPad;
};

void applySkin(inout vec3 position, inout vec3 normal) {
    if (skinEnabled == 0) {
        return;
    }

    int maxIndex = max(boneCount - 1, 0);
    vec4 accumulated = vec4(0.0);
    vec3 accumulatedNormal = vec3(0.0);
    float total = 0.0;
    for (int i = 0; i < 4; ++i) {
        float w = inBoneWeights[i];
        if (w <= 0.0) {
            continue;
        }
        int index = clamp(int(inBoneIndices[i]), 0, maxIndex);
        mat4 bone = bones[index];
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
    position = (objectMatrix * vec4(skinnedPosition, 1.0)).xyz;
    normal = mat3(objectNormalMatrix) * skinnedNormal;
}

layout(location = 0) out vec3 vNormalColor;

void main() {
    vec3 position = inPosition;
    vec3 normal = inNormal;
    applySkin(position, normal);

    vec3 n = normalize(mat3(normalMatrix) * normal);
    vNormalColor = n * 0.5 + 0.5;
    gl_Position = projection * view * model * vec4(position, 1.0);
}
