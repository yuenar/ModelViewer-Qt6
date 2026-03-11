#version 450 core

layout(std140, binding = 0) uniform SelectionUBO {
    vec4 pickingColor;
};

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = pickingColor;
} 
