#version 450

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 n = normalize(fragNormal);
    vec3 L = normalize(vec3(0.3, 1.0, 0.4));
    float ndl = max(dot(n, L), 0.15);
    outColor = vec4(fragColor * ndl, 1.0);
}
