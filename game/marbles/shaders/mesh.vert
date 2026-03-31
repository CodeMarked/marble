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
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;

void main() {
    mat3 nMat = mat3(pc.model);
    fragNormal = nMat * inNormal;
    fragColor = inColor * pc.albedo.rgb;
    gl_Position = ubo.viewProj * pc.model * vec4(inPos, 1.0);
}
