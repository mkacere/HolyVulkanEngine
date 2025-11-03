#version 450

layout (set = 0, binding = 1) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invView;
    mat4 invProjection;
    vec4 position;
    vec4 direction;
    vec2 nearFar;
    vec2 screenSize;
    float fov;
    float aspectRatio;
    uint _pad0;
    uint _pad1;
} Camera;

layout (set = 1, binding = 0) uniform sampler2D uEquirect;

layout (location = 0) out vec4 outColor;

// Convert world-space direction to equirectangular UV
vec2 dirToEquirectUV(vec3 d) {
    d = normalize(d);
    float u = 0.5 + atan(d.z, d.x) / (2.0 * 3.14159265359);
    float v = 0.5 - asin(d.y) / 3.14159265359;
    return vec2(u, v);
}

void main() {
    // Pixel to NDC (-1..1)
    vec2 ndc;
    ndc.x = (gl_FragCoord.x / Camera.screenSize.x) * 2.0 - 1.0;
    ndc.y = (gl_FragCoord.y / Camera.screenSize.y) * 2.0 - 1.0;

    // Reconstruct world-space direction
    vec4 clip = vec4(ndc, 1.0, 1.0);
    vec4 view = Camera.invProjection * clip;
    view /= view.w;
    vec3 dirWS = normalize(mat3(Camera.invView) * view.xyz);

    vec2 uv = dirToEquirectUV(dirWS);
    vec3 color = texture(uEquirect, uv).rgb;
    outColor = vec4(color, 1.0);
}

