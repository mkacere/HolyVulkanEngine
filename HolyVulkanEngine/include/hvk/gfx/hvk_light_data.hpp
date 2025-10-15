#ifndef HVK_LIGHT_DATA_HPP
#define HVK_LIGHT_DATA_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vector>
#include <cstdint>
#include <cstring>

namespace hvk {

/**
 * LightType - Enum for light types
 */
enum class LightType : uint32_t {
    Directional = 0,  // Infinite distance, parallel rays (sun)
    Point = 1,        // Omni-directional, attenuates with distance
    Spot = 2,         // Cone-shaped, attenuates with distance and angle
    Area = 3          // Future: area lights (quad, sphere)
};

/**
 * Light - GPU-friendly light structure
 *
 * Shader binding: set = 0, binding = 2, std430 (in SSBO)
 *
 * Design notes:
 * - std430 layout (tighter packing than std140)
 * - Unified structure for all light types (uses 'type' field)
 * - Directional: uses direction, ignores position, range = 0
 * - Point: uses position, ignores direction
 * - Spot: uses both position and direction, plus cone angles
 * - Each light is 64 bytes (4x vec4)
 */
struct Light {
    // --- Position / Type (16 bytes) ---
    glm::vec4 position;     // xyz = world position (Point, Spot), w = type (cast to LightType)

    // --- Direction / Range (16 bytes) ---
    glm::vec4 direction;    // xyz = direction (Directional, Spot), w = range (0 = infinite)

    // --- Color / Intensity (16 bytes) ---
    glm::vec4 color;        // rgb = light color, a = intensity

    // --- Parameters (16 bytes) ---
    glm::vec4 params;       // x = innerConeAngle (cos), y = outerConeAngle (cos), z/w = reserved

    // Default constructor (invalid light)
    Light()
        : position(0.0f), direction(0.0f), color(0.0f), params(0.0f)
    {}

    // --- Factory Methods ---

    /**
     * Create a directional light (sun, moon, etc.)
     */
    static Light makeDirectional(const glm::vec3& dir, const glm::vec3& col, float intensity = 1.0f) {
        Light light;
        light.position = glm::vec4(0.0f, 0.0f, 0.0f, static_cast<float>(LightType::Directional));
        light.direction = glm::vec4(glm::normalize(dir), 0.0f); // range = 0 (infinite)
        light.color = glm::vec4(col, intensity);
        light.params = glm::vec4(0.0f);
        return light;
    }

    /**
     * Create a point light (light bulb, torch, etc.)
     */
    static Light makePoint(const glm::vec3& pos, const glm::vec3& col, float intensity = 1.0f, float range = 10.0f) {
        Light light;
        light.position = glm::vec4(pos, static_cast<float>(LightType::Point));
        light.direction = glm::vec4(0.0f, 0.0f, 0.0f, range);
        light.color = glm::vec4(col, intensity);
        light.params = glm::vec4(0.0f);
        return light;
    }

    /**
     * Create a spot light (flashlight, street lamp, etc.)
     */
    static Light makeSpot(
        const glm::vec3& pos,
        const glm::vec3& dir,
        const glm::vec3& col,
        float intensity,
        float range,
        float innerConeAngle,  // in radians
        float outerConeAngle   // in radians
    ) {
        Light light;
        light.position = glm::vec4(pos, static_cast<float>(LightType::Spot));
        light.direction = glm::vec4(glm::normalize(dir), range);
        light.color = glm::vec4(col, intensity);
        // Store cosine of angles for efficient shader comparison
        light.params = glm::vec4(glm::cos(innerConeAngle), glm::cos(outerConeAngle), 0.0f, 0.0f);
        return light;
    }

    // --- Accessors ---

    LightType getType() const {
        return static_cast<LightType>(static_cast<uint32_t>(position.w));
    }

    bool isDirectional() const { return getType() == LightType::Directional; }
    bool isPoint() const { return getType() == LightType::Point; }
    bool isSpot() const { return getType() == LightType::Spot; }

    glm::vec3 getPosition() const { return glm::vec3(position); }
    glm::vec3 getDirection() const { return glm::vec3(direction); }
    glm::vec3 getColor() const { return glm::vec3(color); }
    float getIntensity() const { return color.a; }
    float getRange() const { return direction.w; }
};

// Size validation (should be 64 bytes)
static_assert(sizeof(Light) == 64, "Light must be 64 bytes for efficient GPU access");

/**
 * LightBuffer - CPU-side container for building light SSBO data
 *
 * This is what you upload to the GPU as an SSBO.
 * The GPU sees:
 *   layout(std430, ...) readonly buffer LightBuffer {
 *       uint lightCount;
 *       Light lights[];
 *   } lightBuffer;
 */
struct LightBuffer {
    uint32_t lightCount;
    std::vector<Light> lights;

    LightBuffer() : lightCount(0) {}

    void clear() {
        lights.clear();
        lightCount = 0;
    }

    void addLight(const Light& light) {
        lights.push_back(light);
        lightCount = static_cast<uint32_t>(lights.size());
    }

    // Get total size in bytes for GPU buffer allocation
    size_t getBufferSize() const {
        // uint32_t lightCount (4 bytes) + padding to 16 bytes + Light[] array
        // std430 aligns uint to 4 bytes, but next element (Light[]) starts at 16-byte boundary
        size_t headerSize = 16; // uint32_t + 12 bytes padding
        size_t lightsSize = lights.size() * sizeof(Light);
        return headerSize + lightsSize;
    }

    // Write data to CPU buffer (for upload)
    void writeTo(void* dst) const {
        uint8_t* ptr = static_cast<uint8_t*>(dst);

        // Write light count
        std::memcpy(ptr, &lightCount, sizeof(uint32_t));
        ptr += 16; // Skip to 16-byte aligned boundary

        // Write lights array
        if (!lights.empty()) {
            std::memcpy(ptr, lights.data(), lights.size() * sizeof(Light));
        }
    }

    // Convenience: get pointer to data (for memcpy)
    const void* data() const { return &lightCount; }

    bool empty() const { return lightCount == 0; }
    size_t size() const { return lights.size(); }
};

/**
 * Common light presets for quick scene setup
 */
namespace LightPresets {

    // Sunlight (warm daylight)
    inline Light Sunlight(const glm::vec3& direction = glm::vec3(-0.5f, -1.0f, -0.5f)) {
        return Light::makeDirectional(direction, glm::vec3(1.0f, 0.95f, 0.8f), 1.2f);
    }

    // Moonlight (cool, dim)
    inline Light Moonlight(const glm::vec3& direction = glm::vec3(0.3f, -1.0f, 0.3f)) {
        return Light::makeDirectional(direction, glm::vec3(0.7f, 0.8f, 1.0f), 0.3f);
    }

    // Generic white point light
    inline Light WhitePointLight(const glm::vec3& position, float range = 10.0f, float intensity = 1.0f) {
        return Light::makePoint(position, glm::vec3(1.0f), intensity, range);
    }

    // Warm indoor light (incandescent bulb)
    inline Light WarmLight(const glm::vec3& position, float range = 10.0f, float intensity = 1.0f) {
        return Light::makePoint(position, glm::vec3(1.0f, 0.8f, 0.6f), intensity, range);
    }

    // Cool fluorescent light
    inline Light CoolLight(const glm::vec3& position, float range = 10.0f, float intensity = 1.0f) {
        return Light::makePoint(position, glm::vec3(0.9f, 0.95f, 1.0f), intensity, range);
    }

    // Flashlight (narrow spot)
    inline Light Flashlight(const glm::vec3& position, const glm::vec3& direction, float range = 15.0f) {
        return Light::makeSpot(position, direction, glm::vec3(1.0f), 1.0f, range,
            glm::radians(15.0f), glm::radians(25.0f));
    }

    // Street lamp (wide spot)
    inline Light StreetLamp(const glm::vec3& position, const glm::vec3& direction = glm::vec3(0, -1, 0)) {
        return Light::makeSpot(position, direction, glm::vec3(1.0f, 0.9f, 0.7f), 1.5f, 20.0f,
            glm::radians(45.0f), glm::radians(60.0f));
    }

} // namespace LightPresets

} // namespace hvk

#endif // HVK_LIGHT_DATA_HPP
