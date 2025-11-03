#ifndef HVK_ECS_COMPONENTS_HPP
#define HVK_ECS_COMPONENTS_HPP

#include <hvk/scene/hvk_transform.hpp>
#include <hvk/scene/hvk_camera.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <entt/entt.hpp>
#include <string>
#include <cstdint>
#include <vector>

namespace hvk {

/**
 * Component Design Principles:
 * - POD (Plain Old Data) where possible for cache efficiency
 * - Components are data-only (logic goes in Systems)
 * - Use handles/indices instead of pointers for resource references
 * - Keep components small and focused
 */

// ============================================================================
// Core Components
// ============================================================================

/**
 * TransformComponent - 3D spatial transform (Pure data)
 *
 * Stores raw transform data. TransformSystem computes matrices
 * and stores them in LocalToWorld component (see render_components.hpp).
 *
 * This keeps the component pure POD with no computed/cached data.
 * EnTT observers detect changes automatically (no dirty flags needed).
 */
struct TransformComponent {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};  // Identity quaternion
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    TransformComponent() = default;

    TransformComponent(const glm::vec3& pos,
                      const glm::quat& rot = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                      const glm::vec3& scl = glm::vec3(1.0f))
        : position(pos), rotation(rot), scale(scl) {}
};

/**
 * MeshComponent - References a renderable mesh
 *
 * Uses handle pattern: stores index into ResourceManager's model pool.
 * Can optionally reference a specific node within the model's scene graph.
 */
struct MeshComponent {
    uint32_t modelHandle = UINT32_MAX;  // Index into ResourceManager::models_
    int32_t nodeIndex = -1;              // Specific node in model (-1 = draw entire model)
    bool visible = true;

    MeshComponent() = default;

    explicit MeshComponent(uint32_t handle, int32_t node = -1)
        : modelHandle(handle), nodeIndex(node) {}
};

/**
 * CameraComponent - Camera projection settings (Pure data)
 *
 * Stores raw camera parameters. CameraSystem computes view/projection matrices
 * and stores them in RenderCamera component (see render_components.hpp).
 *
 * TransformComponent provides position/orientation.
 * This component only stores projection settings.
 */
struct CameraComponent {
    enum class Type { Perspective, Orthographic };

    Type type = Type::Perspective;
    bool active = true;  // Is this the active rendering camera?

    // Perspective settings
    float fovYDegrees = 45.0f;

    // Orthographic settings
    float orthoWidth = 10.0f;

    // Common settings
    float aspectRatio = 16.0f / 9.0f;
    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    CameraComponent() = default;

    // Factory methods
    static CameraComponent createPerspective(
        float fovYDegrees = 45.0f,
        float aspectRatio = 16.0f / 9.0f,
        float nearPlane = 0.1f,
        float farPlane = 1000.0f
    ) {
        CameraComponent comp;
        comp.type = Type::Perspective;
        comp.fovYDegrees = fovYDegrees;
        comp.aspectRatio = aspectRatio;
        comp.nearPlane = nearPlane;
        comp.farPlane = farPlane;
        return comp;
    }

    static CameraComponent createOrthographic(
        float orthoWidth = 10.0f,
        float aspectRatio = 16.0f / 9.0f,
        float nearPlane = 0.1f,
        float farPlane = 1000.0f
    ) {
        CameraComponent comp;
        comp.type = Type::Orthographic;
        comp.orthoWidth = orthoWidth;
        comp.aspectRatio = aspectRatio;
        comp.nearPlane = nearPlane;
        comp.farPlane = farPlane;
        return comp;
    }
};

// ============================================================================
// Light Components
// ============================================================================

/**
 * PointLightComponent - Omnidirectional point light
 */
struct PointLightComponent {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;  // Attenuation radius

    PointLightComponent() = default;

    PointLightComponent(const glm::vec3& col, float intens, float rad = 10.0f)
        : color(col), intensity(intens), radius(rad) {}
};

/**
 * DirectionalLightComponent - Directional light (like sun)
 *
 * Direction comes from TransformComponent's forward vector
 */
struct DirectionalLightComponent {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;

    DirectionalLightComponent() = default;

    DirectionalLightComponent(const glm::vec3& col, float intens)
        : color(col), intensity(intens) {}
};

/**
 * SpotLightComponent - Cone-shaped spot light
 *
 * Direction comes from TransformComponent's forward vector
 */
struct SpotLightComponent {
    glm::vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
    float radius = 10.0f;
    float innerConeAngle = 15.0f;  // Degrees
    float outerConeAngle = 30.0f;  // Degrees

    SpotLightComponent() = default;

    SpotLightComponent(const glm::vec3& col, float intens, float rad,
                       float innerAngle, float outerAngle)
        : color(col), intensity(intens), radius(rad),
          innerConeAngle(innerAngle), outerConeAngle(outerAngle) {}
};

// ============================================================================
// Hierarchy Components
// ============================================================================

/**
 * ParentComponent - Reference to parent entity
 *
 * When present, this entity's transform is relative to its parent.
 * HierarchySystem computes world-space transform by combining with parent's LocalToWorld.
 */
struct ParentComponent {
    entt::entity parent = entt::null;

    ParentComponent() = default;
    explicit ParentComponent(entt::entity p) : parent(p) {}
};

/**
 * ChildrenComponent - List of child entities
 *
 * Automatically maintained by Scene::setParent() / Scene::removeParent().
 * Used for efficient hierarchy traversal and cascade operations (e.g., delete with children).
 */
struct ChildrenComponent {
    std::vector<entt::entity> children;

    ChildrenComponent() = default;
};

// ============================================================================
// Metadata Components
// ============================================================================

/**
 * NameComponent - Human-readable entity name
 */
struct NameComponent {
    std::string name;

    NameComponent() = default;

    explicit NameComponent(const std::string& n) : name(n) {}
    explicit NameComponent(std::string&& n) : name(std::move(n)) {}
};

// ============================================================================
// Billboard Components
// ============================================================================

/**
 * BillboardMode - How billboard orients toward camera
 */
enum class BillboardMode {
    Spherical,      // Fully faces camera (particles, lights, sprites)
    Cylindrical,    // Rotates around Y-axis only (trees, characters - stays upright)
    ScreenAligned   // No rotation, screen-space aligned (UI overlays)
};

/**
 * BillboardComponent - Always-facing-camera quad
 *
 * Use cases:
 * - Light visualization (glowing spheres at light positions)
 * - Particles (fire, smoke, sparks)
 * - Text/UI in world space (health bars, damage numbers, nameplates)
 * - Impostors (distant trees, rocks)
 *
 * Design:
 * - Single quad (4 vertices, 2 triangles) generated in vertex shader
 * - Orientation computed from camera view matrix
 * - Supports textured or solid color rendering
 * - Alpha blending for transparency/glows
 */
struct BillboardComponent {
    BillboardMode mode = BillboardMode::Spherical;

    glm::vec2 size{1.0f, 1.0f};  // Width, height in world units
    glm::vec4 color{1.0f};        // RGBA color (tint if textured, solid if no texture)

    uint32_t textureHandle = UINT32_MAX;  // Optional texture (INVALID = solid color)
    glm::vec4 uvRect{0.0f, 0.0f, 1.0f, 1.0f};  // UV coordinates (for atlas support)

    bool visible = true;

    // Blend mode hints
    bool additiveBlend = false;  // Use additive blending (for glows/particles)

    BillboardComponent() = default;

    // Convenience: Solid color billboard
    static BillboardComponent createSolid(
        const glm::vec2& size,
        const glm::vec4& color,
        BillboardMode mode = BillboardMode::Spherical,
        bool additive = false
    ) {
        BillboardComponent bb;
        bb.size = size;
        bb.color = color;
        bb.mode = mode;
        bb.additiveBlend = additive;
        return bb;
    }

    // Convenience: Textured billboard
    static BillboardComponent createTextured(
        const glm::vec2& size,
        uint32_t textureHandle,
        const glm::vec4& tint = glm::vec4(1.0f),
        BillboardMode mode = BillboardMode::Spherical
    ) {
        BillboardComponent bb;
        bb.size = size;
        bb.color = tint;
        bb.textureHandle = textureHandle;
        bb.mode = mode;
        return bb;
    }
};

// ============================================================================
// Tag Components (empty structs for filtering)
// ============================================================================

/**
 * Static - Entity doesn't move (optimization hint)
 */
struct StaticTag {};

/**
 * Active - Entity is active in the scene
 */
struct ActiveTag {};

/**
 * Skybox - Special rendering for skybox
 */
struct SkyboxTag {};

} // namespace hvk

#endif // HVK_ECS_COMPONENTS_HPP
