#ifndef HVK_ECS_RENDER_COMPONENTS_HPP
#define HVK_ECS_RENDER_COMPONENTS_HPP

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <cstdint>

namespace hvk {

/**
 * Render Components - Vulkan-specific rendering data
 *
 * These components are managed by RenderWorld and should NOT be
 * directly manipulated by game logic. They are computed/updated
 * from pure game components (TransformComponent, CameraComponent, etc.)
 *
 * Separation rationale:
 * - Game logic operates on pure data (position, rotation, FOV, etc.)
 * - Rendering systems compute derived data (matrices, frustums, etc.)
 * - Keeps ECS layer independent of Vulkan
 */

// ============================================================================
// Transform Rendering Data
// ============================================================================

/**
 * LocalToWorld - Computed transform matrix
 *
 * Automatically computed by TransformSystem from TransformComponent.
 * Used by rendering systems to build push constants.
 *
 * Separation: TransformComponent has raw data (pos, rot, scale),
 * LocalToWorld has computed matrix (for rendering).
 */
struct LocalToWorld {
    glm::mat4 matrix{1.0f};       // Local-to-world transform
    glm::mat4 normalMatrix{1.0f}; // For transforming normals (inverse transpose)

    LocalToWorld() = default;
    explicit LocalToWorld(const glm::mat4& m, const glm::mat4& n = glm::mat4(1.0f))
        : matrix(m), normalMatrix(n) {}
};

// ============================================================================
// Camera Rendering Data
// ============================================================================

/**
 * RenderCamera - Computed camera matrices and frustum
 *
 * Automatically computed by CameraSystem from CameraComponent + TransformComponent.
 * Used by rendering systems for view/projection and culling.
 *
 * Separation: CameraComponent has pure data (FOV, near, far),
 * RenderCamera has computed matrices (for rendering).
 */
struct RenderCamera {
    glm::mat4 view{1.0f};          // View matrix (world → camera space)
    glm::mat4 projection{1.0f};    // Projection matrix (camera → clip space)
    glm::mat4 viewProjection{1.0f}; // Combined view-projection matrix

    // Frustum planes (for culling) - optional, can be computed on-demand
    // Format: vec4(A, B, C, D) where Ax + By + Cz + D = 0
    glm::vec4 frustumPlanes[6];

    RenderCamera() = default;
};

// ============================================================================
// Mesh Rendering Data
// ============================================================================

/**
 * RenderMesh - Vulkan-specific mesh rendering data
 *
 * Automatically created/updated by RenderWorld when MeshComponent is added.
 * Contains Vulkan handles needed for drawing.
 *
 * Separation: MeshComponent has resource handle (which model to use),
 * RenderMesh has Vulkan data (buffers, descriptors, material).
 */
struct RenderMesh {
    // Vulkan buffer handles (from Model)
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    uint32_t indexCount = 0;

    // Material descriptor set (Set 1)
    VkDescriptorSet materialDescriptorSet = VK_NULL_HANDLE;

    // Visibility (computed by culling system)
    bool visible = true;

    // Sort key for render queue optimization (material, depth, etc.)
    uint64_t sortKey = 0;

    RenderMesh() = default;
};

// ============================================================================
// Light Rendering Data
// ============================================================================

/**
 * RenderLight - Light data ready for GPU upload
 *
 * Automatically computed by LightCollectorSystem from light components.
 * Used to fill the GPU light buffer.
 *
 * Separation: Light components have pure data (color, intensity),
 * RenderLight has GPU-ready format (padded, transformed, etc.).
 */
struct RenderPointLight {
    glm::vec3 position{0.0f};  // World position
    float radius = 0.0f;

    glm::vec3 color{1.0f};
    float intensity = 0.0f;

    RenderPointLight() = default;
};

struct RenderDirectionalLight {
    glm::vec3 direction{0.0f, -1.0f, 0.0f};  // World direction (normalized)
    float _pad0 = 0.0f;

    glm::vec3 color{1.0f};
    float intensity = 0.0f;

    RenderDirectionalLight() = default;
};

struct RenderSpotLight {
    glm::vec3 position{0.0f};
    float radius = 0.0f;

    glm::vec3 direction{0.0f, -1.0f, 0.0f};
    float innerConeAngle = 0.0f;

    glm::vec3 color{1.0f};
    float outerConeAngle = 0.0f;

    RenderSpotLight() = default;
};

// ============================================================================
// Tag Components for Rendering
// ============================================================================

/**
 * Visible - Entity should be rendered
 *
 * Added by culling systems after frustum/occlusion testing.
 * Rendering systems only process entities with this tag.
 */
struct VisibleTag {};

/**
 * CastsShadow - Entity casts shadows
 */
struct CastsShadowTag {};

/**
 * ReceivesShadow - Entity receives shadows
 */
struct ReceivesShadowTag {};

} // namespace hvk

#endif // HVK_ECS_RENDER_COMPONENTS_HPP
