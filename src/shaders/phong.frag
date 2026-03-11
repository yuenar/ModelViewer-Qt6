#version 460

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 vTexCoord;

layout(location = 0) out vec4 outColor;

layout(std140, binding = 0) uniform FrameUBO {
    mat4 model;
    mat4 view;
    mat4 projection;
    mat4 normalMatrix;
    vec3 lightPos;   float _p0;
    vec3 lightColor; float _p1;
    vec3 viewPos;    float _p2;
};

layout(std140, binding = 1) uniform MaterialUBO {
    vec3 ambient;  float _m0;
    vec3 diffuse;  float _m1;
    vec3 specular; float _m2;
    float shininess;
};

void main() {
    vec3 color = vec3(1.0, 0.5, 0.2); // 橙色
    vec3 ambientColor = ambient * lightColor * 0.1;
    
    vec3 norm = normalize(fragNormal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuseColor = diffuse * diff * lightColor;
    
    vec3 viewDir = normalize(viewPos - fragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specularColor = specular * spec * lightColor;
    
    outColor = vec4(ambientColor + diffuseColor + specularColor, 1.0);
}
