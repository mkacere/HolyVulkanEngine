#version 450

// Global descriptor set (Set 0) - matches hvk_global_descriptors.hpp
layout(set = 0, binding = 1) uniform CameraData {
    mat4 view;
    mat4 proj;
    mat4 viewProj;
    vec3 position;
} camera;

// Vertex inputs
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

// Outputs to fragment shader
layout(location = 0) out vec3 fragColor;

void main() {
    // Transform position to clip space
    gl_Position = camera.viewProj * vec4(inPosition, 1.0);

    // Pass color to fragment shader
    fragColor = inColor;
}
