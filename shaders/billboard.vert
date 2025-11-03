#version 450

// Billboard vertex shader
// Generates billboard quad in view space, applies orientation mode

// ============================================================================
// Inputs
// ============================================================================

// Vertex attributes (shared quad: 4 corners)
layout(location = 0) in vec2 inCorner;  // (-1,-1), (1,-1), (-1,1), (1,1)

// Instance attributes (one per billboard)
layout(location = 1) in vec3 inPosition;   // World position
layout(location = 2) in vec4 inColor;      // RGBA color
layout(location = 3) in vec2 inSize;       // Width, height
layout(location = 4) in uint inMode;       // Billboard mode (0=Spherical, 1=Cylindrical, 2=ScreenAligned)
layout(location = 5) in vec4 inUVRect;     // UV coordinates (x, y, width, height)

// Global descriptors (Set 0)
layout(set = 0, binding = 0) uniform SceneData {
    vec4 ambientColor;
    vec4 fogColor;
    float fogDensity;
    float time;
} scene;

layout(set = 0, binding = 1) uniform CameraData {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec3 position;
} camera;

// ============================================================================
// Outputs
// ============================================================================

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outColor;

// ============================================================================
// Main
// ============================================================================

void main() {
    // Extract camera right and up vectors from view matrix
    vec3 cameraRight = vec3(camera.view[0][0], camera.view[1][0], camera.view[2][0]);
    vec3 cameraUp = vec3(camera.view[0][1], camera.view[1][1], camera.view[2][1]);
    vec3 cameraForward = vec3(camera.view[0][2], camera.view[1][2], camera.view[2][2]);

    // Apply billboard mode
    vec3 right, up;

    if (inMode == 0u) {
        // Spherical: Fully face camera
        right = cameraRight;
        up = cameraUp;
    }
    else if (inMode == 1u) {
        // Cylindrical: Rotate around Y-axis only (stays upright)
        vec3 toCamera = normalize(camera.position - inPosition);
        right = normalize(cross(vec3(0, 1, 0), toCamera));
        up = vec3(0, 1, 0);
    }
    else {
        // ScreenAligned: Use camera axes without billboard position offset
        right = cameraRight;
        up = cameraUp;
    }

    // Generate billboard quad vertex position
    vec3 worldPos = inPosition
                  + right * inCorner.x * inSize.x * 0.5
                  + up * inCorner.y * inSize.y * 0.5;

    // Transform to clip space
    gl_Position = camera.viewProjection * vec4(worldPos, 1.0);

    // Calculate UV coordinates from corner and UV rect
    // inCorner goes from (-1,-1) to (1,1), remap to (0,0) to (1,1)
    vec2 cornerUV = inCorner * 0.5 + 0.5;
    outUV = inUVRect.xy + cornerUV * inUVRect.zw;

    // Pass color through
    outColor = inColor;
}
