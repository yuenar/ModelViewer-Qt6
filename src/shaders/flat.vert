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
    mat4 bones[64];
    mat4 objectMatrix;
    mat4 objectNormalMatrix;
    int boneCount;
    int skinEnabled;
    vec2 _skinPad;
};

void applySkin(inout vec3 position) {
    if (skinEnabled == 0) {
        return;
    }

    int maxIndex = max(boneCount - 1, 0);
    vec4 accumulated = vec4(0.0);
    float total = 0.0;
    for (int i = 0; i < 4; ++i) {
        float w = inBoneWeights[i];
        if (w <= 0.0) {
            continue;
        }
        int index = clamp(int(inBoneIndices[i]), 0, maxIndex);
        accumulated += w * (bones[index] * vec4(position, 1.0));
        total += w;
    }

    if (total > 0.0) {
        position = accumulated.xyz / total;
    }
    position = (objectMatrix * vec4(position, 1.0)).xyz;
}

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec2 vTexCoord;

void main() {
    vec3 position = inPosition;
    applySkin(position);

    vec4 worldPos = model * vec4(position, 1.0);
    fragPos       = worldPos.xyz;
    vTexCoord     = inTexCoord;
    gl_Position   = projection * view * worldPos;
}
