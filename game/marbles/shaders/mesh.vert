#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(set = 0, binding = 0) uniform UBO {
    mat4 viewProj;
} ubo;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 albedo;
    uint flags;
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec3 fragWorldPos;

void main() {
    vec3 litBase = inColor * pc.albedo.rgb;
    if ((pc.flags & 1u) != 0u) {
        gl_Position = vec4(inPos.xy, 0.999, 1.0);
        fragNormal = vec3(0.0, 0.0, 1.0);
        fragColor = litBase;
        fragWorldPos = vec3(0.0);
    } else {
        mat3 nMat = mat3(pc.model);
        fragNormal = nMat * inNormal;
        fragColor = litBase;
        vec4 wp = pc.model * vec4(inPos, 1.0);
        fragWorldPos = wp.xyz;
        gl_Position = ubo.viewProj * wp;
    }
}
