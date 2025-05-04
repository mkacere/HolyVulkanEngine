#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;     // if you still want per‐vertex color
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec2 inUV;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 normal;
} pc;

layout(set = 0, binding = 0) uniform GlobalUbo {
    mat4 projection;
    mat4 view;
    mat4 inverseView;
    vec4 ambientLightColor;
    int   numLights;
} ubo;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 fragUV;

void main() {
    // Transform normal into world (or view) space:
    fragNormal = (pc.normal * vec4(inNormal, 0.0)).xyz;
    // pass along vertex color if you like:
    fragColor = inColor;
    // pass UV for sampling
    fragUV = inUV;
    // standard MVP:
    gl_Position = ubo.projection * ubo.view * pc.model * vec4(inPosition, 1.0);
}
