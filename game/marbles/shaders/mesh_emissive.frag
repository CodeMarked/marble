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
        outColor = vec4(fragColor * pc.albedo.rgb, pc.albedo.a);
        return;
    }
    vec3 glow = fragColor * pc.albedo.rgb * 1.35;
    outColor = vec4(glow, pc.albedo.a);
}
