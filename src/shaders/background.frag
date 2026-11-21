#version 460

layout(std140, binding = 0) uniform BackgroundUBO {
    vec4 top_color;
    vec4 bot_color;
};

layout(location = 0) in  vec2 v_uv;
layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = mix(bot_color, top_color, clamp(v_uv.y, 0.0, 1.0));
}
