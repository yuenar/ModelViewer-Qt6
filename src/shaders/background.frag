#version 450 core

layout(std140, binding = 0) uniform BackgroundUBO {
    vec4 top_color;
    vec4 bot_color;
};
layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 frag_color;
void main()
{
    frag_color = bot_color * (1 - v_uv.y) + top_color * v_uv.y;
}