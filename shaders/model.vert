#version 450

layout(location=0) in vec3 inPosition;
layout(location=1) in vec3 inColor;
layout(location=2) in vec3 inNormal;

layout(set = 0, binding = 0) uniform MVP {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) out vec3 fragPosWorld;
layout(location = 1) out vec3 fragNormalWorld;
layout(location = 2) out vec3 fragColor;

// how far down (in world units) to move the mesh
const float OFFSET_Y = 0.75;
const float OFFSET_Z = 0.5;

void main() {
    // 1) Reflect across Z axis: invert X and Y, leave Z
    vec3 posReflected   = vec3(inPosition.x, -inPosition.y, -inPosition.z);
    vec3 normReflected  = vec3(inNormal.x,   -inNormal.y,   inNormal.z);

    // 2) Transform into world space
    vec4 worldPos = ubo.model * vec4(posReflected, 1.0);

    // 3) Translate down along Y
    worldPos.y += OFFSET_Y;
    worldPos.z += OFFSET_Z;

    // 4) Compute correct world‐space normal
    mat3 normalMat = transpose(inverse(mat3(ubo.model)));
    vec3 n = normalize(normalMat * normReflected);

    // 5) Feed varyings
    fragPosWorld    = worldPos.xyz;
    fragNormalWorld = n;
    fragColor       = inColor;

    // 6) Final MVP
    gl_Position = ubo.proj * ubo.view * worldPos;
}
