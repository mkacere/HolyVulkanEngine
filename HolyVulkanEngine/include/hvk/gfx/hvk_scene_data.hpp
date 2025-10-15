#ifndef HVK_SCENE_DATA_HPP
#define HVK_SCENE_DATA_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <cstdint>

namespace hvk {

/**
 * SceneData - Global scene-level uniforms (rarely changes)
 *
 * Update frequency: Per scene load, or slowly (environment changes)
 * Shader binding: set = 0, binding = 0, std140
 *
 * Contains:
 * - Ambient lighting
 * - Fog parameters
 * - Global timing for animations
 * - Frame counter
 *
 * Design notes:
 * - std140 layout with explicit padding
 * - Groups data by semantic purpose
 * - Extensible: add IBL, exposure, etc. later
 */
struct SceneData {
    // --- Ambient Lighting (16 bytes) ---
    glm::vec4 ambientColor;         // rgb = color, a = intensity

    // --- Fog (16 bytes) ---
    glm::vec4 fogColor;             // rgb = color, a = density

    // --- Fog Range (8 bytes) ---
    glm::vec2 fogRange;             // x = near distance, y = far distance

    // --- Timing (16 bytes, aligned) ---
    float     time;                 // absolute time in seconds since start
    float     deltaTime;            // delta time in seconds (for this frame)

    // --- Frame Counter (16 bytes, aligned) ---
    uint32_t  frameCount;           // total frames rendered
    uint32_t  _pad0;                // padding
    uint32_t  _pad1;
    uint32_t  _pad2;

    // --- Future expansion slots (optional, commented for now) ---
    // glm::vec4 iblParams;         // x = intensity, y = lod bias, z/w = unused
    // glm::vec4 tonemapParams;     // x = exposure, y = gamma, z/w = unused

    // Default constructor
    SceneData()
        : ambientColor(0.1f, 0.1f, 0.1f, 1.0f)
        , fogColor(0.5f, 0.5f, 0.5f, 0.01f)
        , fogRange(10.0f, 100.0f)
        , time(0.0f)
        , deltaTime(0.0f)
        , frameCount(0)
        , _pad0(0), _pad1(0), _pad2(0)
    {}

    // Update timing each frame
    void updateTiming(float absoluteTime, float dt) {
        time = absoluteTime;
        deltaTime = dt;
        frameCount++;
    }

    // Set ambient light
    void setAmbient(const glm::vec3& color, float intensity = 1.0f) {
        ambientColor = glm::vec4(color, intensity);
    }

    // Set fog parameters
    void setFog(const glm::vec3& color, float density, float nearDist, float farDist) {
        fogColor = glm::vec4(color, density);
        fogRange = glm::vec2(nearDist, farDist);
    }

    // Disable fog
    void disableFog() {
        fogColor.a = 0.0f; // density = 0
    }
};

// Size validation (std140 alignment)
static_assert(sizeof(SceneData) % 16 == 0, "SceneData must be aligned to 16 bytes for std140");

} // namespace hvk

#endif // HVK_SCENE_DATA_HPP
