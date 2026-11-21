#version 460

layout(location = 0) in vec2 vPos;

layout(location = 0) out vec4 outColor;

layout(std140, binding = 0) uniform PostUBO {
    vec2 texelSize;
    float flipY;          // > 0.5: NDC +Y 朝上
    float aoStrength;
    float nearPlane;
    float farPlane;
    float aoRadius;
    float fxaaEnabled;
    float aoEnabled;
    float exposure;
    float padding0;
    float padding1;
};

layout(binding = 1) uniform sampler2D sceneColor;
layout(binding = 2) uniform sampler2D sceneDepth;

float luma(vec3 color) {
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec2 uvOf(vec2 pos) {
    vec2 uv = pos * 0.5 + 0.5;
    if (flipY > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    return uv;
}

/// 轻量 FXAA（基于亮度边缘的 3x3 采样）
vec3 applyFxaa(vec2 uv, vec3 center) {
    vec2 t = texelSize;
    vec3 nw = texture(sceneColor, uv + vec2(-1.0, -1.0) * t).rgb;
    vec3 ne = texture(sceneColor, uv + vec2( 1.0, -1.0) * t).rgb;
    vec3 sw = texture(sceneColor, uv + vec2(-1.0,  1.0) * t).rgb;
    vec3 se = texture(sceneColor, uv + vec2( 1.0,  1.0) * t).rgb;

    float lNW = luma(nw);
    float lNE = luma(ne);
    float lSW = luma(sw);
    float lSE = luma(se);
    float lM = luma(center);

    float lMin = min(lM, min(min(lNW, lNE), min(lSW, lSE)));
    float lMax = max(lM, max(max(lNW, lNE), max(lSW, lSE)));

    if (lMax - lMin < max(0.0312, lMax * 0.125)) {
        return center;
    }

    vec2 dir = vec2(-((lNW + lNE) - (lSW + lSE)),
                     ((lNW + lSW) - (lNE + lSE)));
    float dirReduce = max((lNW + lNE + lSW + lSE) * 0.25 * 0.03125,
                          1.0 / 128.0);
    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
    dir = clamp(dir * rcpDirMin, vec2(-8.0), vec2(8.0)) * t;

    vec3 rgbA = 0.5 * (texture(sceneColor, uv + dir * (1.0 / 3.0 - 0.5)).rgb +
                       texture(sceneColor, uv + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 +
                0.25 * (texture(sceneColor, uv - dir * 0.5).rgb +
                        texture(sceneColor, uv + dir * 0.5).rgb);

    float lB = luma(rgbB);
    return (lB < lMin || lB > lMax) ? rgbA : rgbB;
}

/// 基于深度的屏幕空间环境光遮蔽（8 次采样）
float applyAo(vec2 uv) {
    if (aoEnabled < 0.5) {
        return 1.0;
    }

    float centerDepth = texture(sceneDepth, uv).r;
    if (centerDepth <= 0.0 || centerDepth >= 1.0) {
        return 1.0;
    }

    const vec2 offsets[8] = vec2[](
        vec2( 1.0,  0.0), vec2(-1.0,  0.0),
        vec2( 0.0,  1.0), vec2( 0.0, -1.0),
        vec2( 0.7071,  0.7071), vec2(-0.7071,  0.7071),
        vec2( 0.7071, -0.7071), vec2(-0.7071, -0.7071));

    float occlusion = 0.0;
    for (int i = 0; i < 8; ++i) {
        vec2 offset = offsets[i] * texelSize * aoRadius;
        float sampleDepth = texture(sceneDepth, uv + offset).r;
        float diff = centerDepth - sampleDepth;   // > 0：邻居更靠近相机
        if (diff > 0.0004 && diff < 0.02) {
            occlusion += 1.0 - diff / 0.02;
        }
    }

    return clamp(1.0 - aoStrength * occlusion / 8.0, 0.0, 1.0);
}

void main() {
    vec2 uv = uvOf(vPos);
    vec3 color = texture(sceneColor, uv).rgb;

    float ao = applyAo(uv);
    color *= ao;

    if (fxaaEnabled > 0.5) {
        color = applyFxaa(uv, color);
    }

    color *= exposure;
    outColor = vec4(color, 1.0);
}
