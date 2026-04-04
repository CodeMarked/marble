#version 450

layout(push_constant) uniform Push {
    mat4 model;
    vec4 albedo;
    uint flags;
} pc;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 0) out vec4 outColor;

void main() {
    if ((pc.flags & 2u) != 0u) {
        outColor = vec4(fragColor, pc.albedo.a);
        return;
    }
    vec3 n = normalize(fragNormal);
    vec3 L = normalize(vec3(0.28, 0.92, 0.32));
    float ndl = max(dot(n, L), 0.12);
    vec3 lit = fragColor * ndl;
    float distXZ = length(fragWorldPos.xz);
    float fogAmt = 1.0 - exp(-distXZ * 0.0028);
    fogAmt = clamp(fogAmt, 0.0, 0.72);
    vec3 fogCol = vec3(0.06, 0.12, 0.11);
    vec3 rgb = mix(lit, fogCol, fogAmt);
    outColor = vec4(rgb, pc.albedo.a);
}
