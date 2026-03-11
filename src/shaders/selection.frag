#version 450 core

uniform vec4 pickingColor;

layout(location = 0) out vec4 fragColor;

void main()
{
    fragColor = pickingColor;
} 
