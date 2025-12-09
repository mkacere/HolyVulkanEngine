#ifndef HVK_ECS_PHYSICS_SYSTEM_HPP
#define HVK_ECS_PHYSICS_SYSTEM_HPP

#include <hvk/ecs/hvk_system.hpp>
#include <hvk/ecs/hvk_physics_components.hpp>
#include <memory>

// Forward declare Jolt types (avoid including in header)
namespace JPH {
    class PhysicsSystem;
    class TempAllocator;
    class JobSystem;
    class BodyInterface;
    class BroadPhaseLayerInterface;
    class ObjectVsBroadPhaseLayerFilter;
    class ObjectLayerPairFilter;
}

namespace hvk {

// Forward declarations
class DebugLineRenderSystem;
class BroadPhaseLayerInterfaceImpl;
class ObjectVsBroadPhaseLayerFilterImpl;
class ObjectLayerPairFilterImpl;

/**
 * PhysicsSystem - Jolt Physics integration
 *
 * Responsibilities:
 * - Initialize Jolt Physics world
 * - Sync ECS transforms → Jolt bodies (before physics step)
 * - Step Jolt simulation at fixed timestep
 * - Sync Jolt bodies → ECS transforms (after physics step)
 * - Handle body creation/destruction when components added/removed
 *
 * System order (within Scene::update):
 * 1. PhysicsSystem - updates TransformComponent from Jolt
 * 2. TransformSystem - computes LocalToWorld matrices
 * 3. HierarchySystem - propagates transforms through hierarchy
 */
class PhysicsSystem : public ISystem {
public:
    PhysicsSystem();
    ~PhysicsSystem() override;

    void init(Scene& scene) override;
    void update(Scene& scene, float deltaTime) override;
    void cleanup() override;

    // Debug visualization
    void setDebugRenderer(DebugLineRenderSystem* debugLines) { debugLines_ = debugLines; }
    void setDebugDrawEnabled(bool enabled) { debugDrawEnabled_ = enabled; }
    bool isDebugDrawEnabled() const { return debugDrawEnabled_; }

private:
    // Jolt Physics objects (opaque pointers to avoid header pollution)
    std::unique_ptr<JPH::TempAllocator> tempAllocator_;
    std::unique_ptr<JPH::JobSystem> jobSystem_;
    std::unique_ptr<JPH::PhysicsSystem> physicsSystem_;

    // Layer filter interfaces (must persist for lifetime of physics system)
    std::unique_ptr<BroadPhaseLayerInterfaceImpl> broadPhaseLayerInterface_;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl> objectVsBroadPhaseLayerFilter_;
    std::unique_ptr<ObjectLayerPairFilterImpl> objectLayerPairFilter_;

    // Helper: Create Jolt body from ECS components
    void createBody(entt::registry& registry, entt::entity entity);

    // Helper: Update Jolt body from ECS components (for kinematic bodies)
    void updateKinematicBody(entt::registry& registry, entt::entity entity);

    // Helper: Sync Jolt body state → ECS components
    void syncBodyToECS(entt::registry& registry, entt::entity entity);

    // Debug visualization
    DebugLineRenderSystem* debugLines_ = nullptr;
    bool debugDrawEnabled_ = false;
    void drawDebugVisualization(entt::registry& registry);
};

} // namespace hvk

#endif // HVK_ECS_PHYSICS_SYSTEM_HPP
