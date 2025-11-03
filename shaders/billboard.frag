#version 450

// Billboard fragment shader
// Creates circular glowing spheres for lights

// ============================================================================
// Inputs
// ============================================================================

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;

// ============================================================================
// Outputs
// ============================================================================

layout(location = 0) out vec4 outColor;

// ============================================================================
// Main
// ============================================================================

void main() {
    // Create circular glow effect
    // UV goes from (0,0) to (1,1), remap to (-1,-1) to (1,1) for centered circle
    vec2 centered = inUV * 2.0 - 1.0;
    float dist = length(centered);

    // Soft circular falloff for glow effect
    // Inner core is bright, outer edge fades out
    float alpha = 1.0 - smoothstep(0.0, 1.0, dist);
    alpha = pow(alpha, 2.0);  // Make falloff sharper

    // Apply alpha to color
    outColor = vec4(inColor.rgb, inColor.a * alpha);

    // Discard pixels outside the circle
    if (dist > 1.0 || outColor.a < 0.01) {
        discard;
    }
}
