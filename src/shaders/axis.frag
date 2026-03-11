#version 450 core

layout( location = 0 ) out vec4 fragColor;

layout(location = 0) in vec3 fragmentColor;

layout(std140, binding = 0) uniform AxisUBO {
    bool renderCone;
    float _p0;
    float _p1;
    float _p2;
    vec3 coneColor;
};

void main()
{
    if(renderCone)
        fragColor = vec4(coneColor, 1.0);
    else
        fragColor = vec4(fragmentColor.xyz, 1.0);
}