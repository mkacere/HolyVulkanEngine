#version 450

const float PI = 3.14159265359;
const int MAX_LIGHTS = 32;

// Set 0, Binding 0: Scene data
layout(set = 0, binding = 0) uniform SceneData {
    vec4 ambientColor;
    vec4 fogColor;
    vec2 fogRange;
    float time;
    float deltaTime;
    uint frameCount;
    float _pad0;
    float _pad1;
    float _pad2;
} scene;

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

// Set 0, Binding 2: Light buffer (SSBO)
struct Light {
    vec4 position;   // xyz = position, w = type (0=directional, 1=point, 2=spot)
    vec4 direction;  // xyz = direction, w = range
    vec4 color;      // rgb = color, a = intensity
    vec4 params;     // spotlight cone angles, etc.
};

layout(set = 0, binding = 2) readonly buffer LightBuffer {
    uint lightCount;
    uint _pad0;
    uint _pad1;
    uint _pad2;
    Light lights[MAX_LIGHTS];
} lightBuffer;

// Set 1: Material textures
layout(set = 1, binding = 0) uniform sampler2D albedoMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;
layout(set = 1, binding = 3) uniform sampler2D emissiveMap;
layout(set = 1, binding = 4) uniform sampler2D occlusionMap;

// Push constants: Model matrices + Material parameters
layout(push_constant) uniform PushConstants {
    mat4 model;              // Model matrix (offset 0, 64 bytes)
    mat4 normalMatrix;       // Normal matrix (offset 64, 64 bytes)

    // Material parameters (offset 128)
    // std140 layout: vec3 emissiveFactor is 12 bytes but ALIGNED to 16 bytes!
    vec4 baseColorFactor;    // RGBA tint for base color (16 bytes, offset 0)
    vec3 emissiveFactor;     // RGB emissive color (12 bytes + 4 implicit padding, offset 16)
    float metallicFactor;    // Metallic multiplier [0, 1] (4 bytes, offset 32)
    float roughnessFactor;   // Roughness multiplier [0, 1] (4 bytes, offset 36)
    float alphaCutoff;       // Alpha test threshold (4 bytes, offset 40)
    uint alphaMode;          // 0=Opaque, 1=Mask, 2=Blend (4 bytes, offset 44)

    // Texture presence flags (no padding needed, pack tightly)
    uint hasBaseColorTex;           // offset 48
    uint hasNormalTex;              // offset 52
    uint hasMetallicRoughnessTex;   // offset 56
    uint hasEmissiveTex;            // offset 60
    uint hasOcclusionTex;           // offset 64
    uint _pad0;                     // offset 68
    uint _pad1;                     // offset 72
    uint _pad2;                     // offset 76
    // Total: 80 bytes (matches C++ MaterialParams)
} pc;

// Input from vertex shader
layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;
layout(location = 4) in vec3 inTangent;
layout(location = 5) in vec3 inBitangent;

// Output
layout(location = 0) out vec4 outColor;

// ============================================================================
// PBR Helper Functions
// ============================================================================

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / denom;
}

// Geometry Function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel Equation (Schlick approximation)
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============================================================================
// Lighting Functions
// ============================================================================

vec3 CalculatePBR(vec3 N, vec3 V, vec3 L, vec3 albedo, float metallic, float roughness, vec3 F0, vec3 radiance) {
    vec3 H = normalize(V + L);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    // Energy conservation
    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

vec3 ProcessLight(Light light, vec3 N, vec3 V, vec3 worldPos, vec3 albedo, float metallic, float roughness, vec3 F0) {
    uint lightType = uint(light.position.w);
    vec3 lightColor = light.color.rgb * light.color.a; // color * intensity

    if (lightType == 0u) {
        // Directional light
        vec3 L = normalize(-light.direction.xyz);
        return CalculatePBR(N, V, L, albedo, metallic, roughness, F0, lightColor);
    }
    else if (lightType == 1u) {
        // Point light
        vec3 lightPos = light.position.xyz;
        vec3 L = normalize(lightPos - worldPos);
        float distance = length(lightPos - worldPos);
        float range = light.direction.w;

        // Attenuation
        float attenuation = 1.0 / (distance * distance);
        if (range > 0.0) {
            float rangeAttenuation = max(0.0, 1.0 - (distance / range));
            attenuation *= rangeAttenuation * rangeAttenuation;
        }

        vec3 radiance = lightColor * attenuation;
        return CalculatePBR(N, V, L, albedo, metallic, roughness, F0, radiance);
    }
    else if (lightType == 2u) {
        // Spot light
        vec3 lightPos = light.position.xyz;
        vec3 L = normalize(lightPos - worldPos);
        vec3 spotDir = normalize(light.direction.xyz);
        float distance = length(lightPos - worldPos);
        float range = light.direction.w;

        // Spot cone
        float innerCone = light.params.x;
        float outerCone = light.params.y;
        float theta = dot(L, -spotDir);
        float epsilon = innerCone - outerCone;
        float spotIntensity = clamp((theta - outerCone) / epsilon, 0.0, 1.0);

        // Attenuation
        float attenuation = 1.0 / (distance * distance);
        if (range > 0.0) {
            float rangeAttenuation = max(0.0, 1.0 - (distance / range));
            attenuation *= rangeAttenuation * rangeAttenuation;
        }

        vec3 radiance = lightColor * attenuation * spotIntensity;
        return CalculatePBR(N, V, L, albedo, metallic, roughness, F0, radiance);
    }

    return vec3(0.0);
}

// ============================================================================
// Main
// ============================================================================

void main() {
    // Sample material textures and apply GLTF material factors
    // Albedo texture is SRGB format, so hardware converts to linear automatically
    vec4 albedoSample = texture(albedoMap, inUV);
    vec3 albedo = albedoSample.rgb * pc.baseColorFactor.rgb;  // GLTF: texture * factor
    float alpha = albedoSample.a * pc.baseColorFactor.a;      // GLTF: alpha = texture.a * factor.a

    // ===== DEBUG MODES (uncomment ONE to diagnose issues) =====
    // DEBUG 1: Raw albedo texture (before factor multiplication)
    // outColor = vec4(albedoSample.rgb, 1.0); return;

    // DEBUG 2: BaseColorFactor (material's base color)
    // outColor = vec4(pc.baseColorFactor.rgb, 1.0); return;

    // DEBUG 3: Final albedo (texture * factor)
    // outColor = vec4(albedo, 1.0); return;

    // DEBUG 4: UV coordinates
    // outColor = vec4(inUV, 0.0, 1.0); return;

    // DEBUG 5: Alpha channel
    // outColor = vec4(vec3(albedoSample.a), 1.0); return;

    // DEBUG 6: Check if using default white texture (will show magenta if default)
    // if (length(albedoSample.rgb - vec3(1.0)) < 0.01) { outColor = vec4(1, 0, 1, 1); return; }

    // DEBUG 7: Color by alphaMode (OPAQUE=green, MASK=blue, BLEND=red)
    // if (pc.alphaMode == 0u) { outColor = vec4(0, 1, 0, 1); return; }      // OPAQUE = green
    // else if (pc.alphaMode == 1u) { outColor = vec4(0, 0, 1, 1); return; } // MASK = blue
    // else if (pc.alphaMode == 2u) { outColor = vec4(1, 0, 0, 1); return; } // BLEND = red
    // ===== END DEBUG MODES =====

    // WORKAROUND: If baseColorFactor is black but we have white default texture, use texture as-is
    // This handles models with black factors that rely entirely on textures
    if (length(pc.baseColorFactor.rgb) < 0.01) {
        // BaseColorFactor is black - check if we have actual texture data
        if (length(albedoSample.rgb - vec3(1.0)) > 0.01) {
            // We have texture data (not default white), use it without factor
            albedo = albedoSample.rgb;
        } else {
            // Default white texture, use baseColorFactor as albedo (will be black)
            albedo = pc.baseColorFactor.rgb;
        }
    }

    // GLTF Alpha Masking: Discard fragments below threshold
    if (pc.alphaMode == 1u) {  // AlphaMode::Mask
        if (alpha < pc.alphaCutoff) {
            discard;  // Don't render this fragment
        }
    }

    vec3 normalSample = texture(normalMap, inUV).rgb;
    vec2 metallicRoughnessSample = texture(metallicRoughnessMap, inUV).bg; // B=metallic, G=roughness
    float metallic = metallicRoughnessSample.x * pc.metallicFactor;        // GLTF: texture.b * factor
    float roughness = max(metallicRoughnessSample.y * pc.roughnessFactor, 0.04); // GLTF: texture.g * factor (clamped)

    vec3 emissive = texture(emissiveMap, inUV).rgb * pc.emissiveFactor;   // GLTF: texture * factor
    float occlusion = texture(occlusionMap, inUV).r;

    // Build TBN matrix for normal mapping
    vec3 T = normalize(inTangent);
    vec3 B = normalize(inBitangent);
    vec3 N = normalize(inNormal);
    mat3 TBN = mat3(T, B, N);

    // Convert normal from tangent space to world space
    vec3 tangentNormal = normalSample * 2.0 - 1.0;
    N = normalize(TBN * tangentNormal);

    // Simple lighting (no PBR for now - just to debug)
    vec3 lighting = vec3(0.0);

    // Directional light (simple Lambert)
    if (lightBuffer.lightCount > 0u) {
        Light dirLight = lightBuffer.lights[0];
        vec3 L = normalize(-dirLight.direction.xyz);
        float NdotL = max(dot(N, L), 0.0);
        vec3 lightColor = dirLight.color.rgb * dirLight.color.a;
        lighting += albedo * lightColor * NdotL;
    }

    // Ambient
    vec3 ambient = scene.ambientColor.rgb * albedo;

    // Final color
    vec3 color = lighting + ambient;

    // No tone mapping - just clamp
    color = clamp(color, 0.0, 1.0);

    // Output with proper alpha (for alpha blending if alphaMode == Blend)
    outColor = vec4(color, alpha);
}
