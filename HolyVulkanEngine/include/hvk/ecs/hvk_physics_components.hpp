#ifndef HVK_ECS_PHYSICS_COMPONENTS_HPP
#define HVK_ECS_PHYSICS_COMPONENTS_HPP

#include <glm/glm.hpp>
#include <cstdint>

namespace hvk {

/**
 * RigidBodyComponent - Physics simulation properties
 *
 * Controls how an entity responds to forces and collisions.
 * Follows component design pattern: POD, data-only.
 */
struct RigidBodyComponent {
    enum class Type {
        Static,      // Immovable (ground, walls)
        Dynamic,     // Moves with physics (boxes, balls)
        Kinematic    // Moves programmatically (platforms, doors)
    };

    Type type = Type::Dynamic;
    float mass = 1.0f;              // Mass in kg (ignored for static/kinematic)
    float friction = 0.5f;          // Surface friction coefficient
    float restitution = 0.3f;       // Bounciness (0 = no bounce, 1 = perfect bounce)
    float linearDamping = 0.05f;    // Air resistance for linear motion
    float angularDamping = 0.05f;   // Air resistance for rotation
    bool useGravity = true;         // Affected by gravity?
    bool isSensor = false;          // Trigger volume (no collision response)

    // Jolt body ID (set by PhysicsSystem, don't modify manually)
    uint32_t joltBodyID = 0xFFFFFFFF;  // INVALID initially

    constexpr RigidBodyComponent() = default;
};

/**
 * ColliderComponent - Collision shape definition
 *
 * Defines the physical shape for collision detection.
 * Position/rotation come from TransformComponent.
 */
struct ColliderComponent {
    enum class Shape {
        Box,        // Box shape (size = half-extents)
        Sphere,     // Sphere shape (size.x = radius)
        Capsule,    // Capsule shape (size.x = radius, size.y = height)
        Cylinder    // Cylinder shape (size.x = radius, size.y = height)
    };

    Shape shape = Shape::Box;
    glm::vec3 center{0.0f};         // Local offset from entity position
    glm::vec3 size{1.0f};           // Shape-specific dimensions

    constexpr ColliderComponent() = default;

    // Convenience factory methods
    static constexpr ColliderComponent createBox(const glm::vec3& halfExtents) {
        ColliderComponent c;
        c.shape = Shape::Box;
        c.size = halfExtents;
        return c;
    }

    static constexpr ColliderComponent createSphere(float radius) {
        ColliderComponent c;
        c.shape = Shape::Sphere;
        c.size = glm::vec3(radius, 0.0f, 0.0f);
        return c;
    }

    static constexpr ColliderComponent createCapsule(float radius, float height) {
        ColliderComponent c;
        c.shape = Shape::Capsule;
        c.size = glm::vec3(radius, height, 0.0f);
        return c;
    }
};

/**
 * VelocityComponent - Physics velocity state
 *
 * Optional component for querying/modifying velocity.
 * Automatically updated by PhysicsSystem from Jolt.
 */
struct VelocityComponent {
    glm::vec3 linear{0.0f};    // Linear velocity (m/s)
    glm::vec3 angular{0.0f};   // Angular velocity (rad/s)

    constexpr VelocityComponent() = default;
};

} // namespace hvk

#endif // HVK_ECS_PHYSICS_COMPONENTS_HPP
