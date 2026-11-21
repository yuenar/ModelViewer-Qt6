#version 460

layout(location = 0) in  vec3 vDirection;
layout(location = 0) out vec4 fragColor;

void main() {
    vec3 dir = normalize(vDirection);

    vec3 horizon = vec3(0.55, 0.62, 0.72);
    vec3 zenith  = vec3(0.15, 0.28, 0.55);
    vec3 ground  = vec3(0.16, 0.15, 0.14);

    float t = clamp(dir.y, -1.0, 1.0);
    vec3 color;
    if (t >= 0.0) {
        color = mix(horizon, zenith, pow(t, 0.8));
    } else {
        color = mix(horizon, ground, pow(-t, 0.8));
    }

    // 模拟太阳方向光晕
    float sun = pow(max(dot(dir, normalize(vec3(0.5, 0.7, 0.4))), 0.0), 256.0);
    color += vec3(1.0, 0.9, 0.7) * sun;

    fragColor = vec4(color, 1.0);
}
