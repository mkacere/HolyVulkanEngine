#version 450

// Set 0, Binding 1: Camera data
layout(set = 0, binding = 1) uniform CameraData {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invView;
    mat4 invProjection;
    vec4 position;      // Camera world position
    vec4 direction;     // Camera forward direction
    vec2 nearFar;
    vec2 screenSize;
    float fov;
    float aspectRatio;
    float _pad0;
    float _pad1;
} camera;

// Push constants: Model matrices + Material parameters
// NOTE: Must match fragment shader layout exactly!
layout(push_constant) uniform PushConstants {
    mat4 model;              // Model matrix (offset 0, 64 bytes)
    mat4 normalMatrix;       // Normal matrix (offset 64, 64 bytes)

    // Material parameters (offset 128) - not used in vertex shader, but must match frag shader
    vec4 baseColorFactor;    // RGBA tint for base color (16 bytes)
    vec3 emissiveFactor;     // RGB emissive color (12 bytes)
    float metallicFactor;    // Metallic multiplier [0, 1] (4 bytes)
    float roughnessFactor;   // Roughness multiplier [0, 1] (4 bytes)
    float alphaCutoff;       // Alpha test threshold (4 bytes)
    uint alphaMode;          // 0=Opaque, 1=Mask, 2=Blend (4 bytes)
    uint _pad0;              // Padding (4 bytes)

    // Texture presence flags (16 bytes)
    uint hasBaseColorTex;
    uint hasNormalTex;
    uint hasMetallicRoughnessTex;
    uint hasEmissiveTex;
    uint hasOcclusionTex;
    uint _pad1;
    uint _pad2;
    uint _pad3;
} push;

// Vertex attributes (must match hvk::Vertex in hvk_mesh.h)
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec4 inTangent;  // xyz = tangent, w = bitangent handedness

// Output to fragment shader
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outUV;
layout(location = 3) out vec4 outColor;
layout(location = 4) out vec3 outTangent;
layout(location = 5) out vec3 outBitangent;

void main() {
    // Transform position to world space
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;

    // Transform position to clip space
    gl_Position = camera.viewProjection * worldPos;

    // Transform normal, tangent, bitangent to world space
    // Use normal matrix to handle non-uniform scaling
    outNormal = normalize(mat3(push.normalMatrix) * inNormal);
    outTangent = normalize(mat3(push.normalMatrix) * inTangent.xyz);

    // Reconstruct bitangent in world space
    // The w component stores the handedness (+1 or -1)
    outBitangent = cross(outNormal, outTangent) * inTangent.w;

    // Pass through UV and vertex color
    outUV = inUV;
    outColor = inColor;
}
