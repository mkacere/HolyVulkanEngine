/**
 * @file hvk_camera_data.hpp
 * @brief Camera uniform data structures
 * @author Holy Vulkan Engine
 * @date 2025
 * Defines camera matrices (view, projection, view-projection) for shaders.
 */

#ifndef HVK_CAMERA_DATA_HPP
#define HVK_CAMERA_DATA_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdint>

namespace hvk {

/**
 * CameraData - Per-view camera uniforms
 *
 * Update frequency: Per frame, per camera/view
 * Shader binding: set = 0, binding = 1, std140
 *
 * Contains:
 * - View and projection matrices (and inverses)
 * - Precomputed viewProjection matrix
 * - Camera position and direction
 * - Near/far planes, FOV, aspect ratio
 * - Screen resolution
 *
 * Design notes:
 * - std140 layout (matrices are column-major, 16-byte aligned)
 * - Provides inverse matrices for screenspace reconstruction
 * - Explicit position/direction for lighting calculations
 * - All commonly needed camera data in one place
 */
struct CameraData {
    // --- Matrices (64 bytes each) ---
    glm::mat4 view;                // world -> view space
    glm::mat4 projection;          // view -> clip space (reversed-Z friendly)
    glm::mat4 viewProjection;      // precomputed: projection * view
    glm::mat4 invView;             // view -> world (for world-space reconstruction)
    glm::mat4 invProjection;       // clip -> view (for screenspace -> viewspace)

    // --- Camera Vectors (32 bytes) ---
    glm::vec4 position;            // xyz = world position, w = unused
    glm::vec4 direction;           // xyz = forward direction (normalized), w = unused

    // --- Camera Parameters (16 bytes) ---
    glm::vec2 nearFar;             // x = near plane, y = far plane
    glm::vec2 screenSize;          // x = width, y = height (in pixels)

    // --- Additional Parameters (16 bytes) ---
    float     fov;                 // vertical field of view (radians)
    float     aspectRatio;         // width / height
    uint32_t  _pad0;
    uint32_t  _pad1;

    // Default constructor
    CameraData()
        : view(1.0f)
        , projection(1.0f)
        , viewProjection(1.0f)
        , invView(1.0f)
        , invProjection(1.0f)
        , position(0.0f, 0.0f, 0.0f, 0.0f)
        , direction(0.0f, 0.0f, -1.0f, 0.0f)
        , nearFar(0.1f, 1000.0f)
        , screenSize(1920.0f, 1080.0f)
        , fov(glm::radians(60.0f))
        , aspectRatio(1920.0f / 1080.0f)
        , _pad0(0), _pad1(0)
    {}

    /**
     * Update camera from position, target, and projection parameters
     *
     * @param pos Camera position in world space
     * @param target Point to look at in world space
     * @param up Up vector (usually (0, 1, 0))
     * @param fovRadians Vertical field of view in radians
     * @param aspect Aspect ratio (width / height)
     * @param nearPlane Near clip plane
     * @param farPlane Far clip plane
     * @param screenWidth Screen width in pixels
     * @param screenHeight Screen height in pixels
     */
    void setLookAt(
        const glm::vec3& pos,
        const glm::vec3& target,
        const glm::vec3& up,
        float fovRadians,
        float aspect,
        float nearPlane,
        float farPlane,
        float screenWidth,
        float screenHeight
    ) {
        // Store position and compute direction
        position = glm::vec4(pos, 0.0f);
        direction = glm::vec4(glm::normalize(target - pos), 0.0f);

        // Build view matrix
        view = glm::lookAt(pos, target, up);
        invView = glm::inverse(view);

        // Build projection matrix (Vulkan NDC: x,y in [-1,1], z in [0,1])
        projection = glm::perspective(fovRadians, aspect, nearPlane, farPlane);

        // GLM was designed for OpenGL where clip space Z is [-1, 1] and Y is inverted
        // For Vulkan, we need to flip Y and use [0, 1] for Z (handled by GLM_FORCE_DEPTH_ZERO_TO_ONE)
        // But we still need to flip Y:
        projection[1][1] *= -1.0f;

        invProjection = glm::inverse(projection);

        // Precompute combined matrix
        viewProjection = projection * view;

        // Store parameters
        nearFar = glm::vec2(nearPlane, farPlane);
        screenSize = glm::vec2(screenWidth, screenHeight);
        fov = fovRadians;
        aspectRatio = aspect;
    }

    /**
     * Update camera from view and projection matrices directly
     * (Useful when integrating with external camera systems)
     */
    void setMatrices(
        const glm::mat4& viewMatrix,
        const glm::mat4& projectionMatrix,
        float screenWidth,
        float screenHeight
    ) {
        view = viewMatrix;
        projection = projectionMatrix;
        viewProjection = projection * view;
        invView = glm::inverse(view);
        invProjection = glm::inverse(projection);

        // Extract position from inverse view matrix
        position = glm::vec4(invView[3].x, invView[3].y, invView[3].z, 0.0f);

        // Extract forward direction from view matrix (negative Z axis)
        direction = glm::vec4(-view[0][2], -view[1][2], -view[2][2], 0.0f);

        screenSize = glm::vec2(screenWidth, screenHeight);
    }

    /**
     * Update only projection matrix (useful for resize)
     */
    void updateProjection(
        float fovRadians,
        float aspect,
        float nearPlane,
        float farPlane,
        float screenWidth,
        float screenHeight
    ) {
        projection = glm::perspective(fovRadians, aspect, nearPlane, farPlane);
        projection[1][1] *= -1.0f; // Flip Y for Vulkan
        invProjection = glm::inverse(projection);
        viewProjection = projection * view;

        nearFar = glm::vec2(nearPlane, farPlane);
        screenSize = glm::vec2(screenWidth, screenHeight);
        fov = fovRadians;
        aspectRatio = aspect;
    }

    /**
     * Get the right vector (X axis in camera space)
     */
    glm::vec3 getRightVector() const {
        return glm::vec3(invView[0].x, invView[0].y, invView[0].z);
    }

    /**
     * Get the up vector (Y axis in camera space)
     */
    glm::vec3 getUpVector() const {
        return glm::vec3(invView[1].x, invView[1].y, invView[1].z);
    }

    /**
     * Get the forward vector (negative Z axis in camera space)
     */
    glm::vec3 getForwardVector() const {
        return glm::vec3(direction.x, direction.y, direction.z);
    }
};

// Size validation (std140 alignment)
static_assert(sizeof(CameraData) % 16 == 0, "CameraData must be aligned to 16 bytes for std140");

} // namespace hvk

#endif // HVK_CAMERA_DATA_HPP
