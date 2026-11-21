#version 460

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec3 vColor;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D labelAtlas;

void main() {
    vec4 texel = texture(labelAtlas, vUV);
    if (texel.a < 0.1) {
        discard;
    }
    outColor = vec4(texel.rgb * vColor, 1.0);
}
