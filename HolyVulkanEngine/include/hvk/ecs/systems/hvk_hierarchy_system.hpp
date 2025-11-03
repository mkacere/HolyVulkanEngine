/**
 * @file hvk_hierarchy_system.hpp
 * @brief Scene hierarchy management system
 * @author Holy Vulkan Engine
 * @date 2025
 * Manages parent-child relationships in the scene graph.
 */

#ifndef HVK_ECS_HIERARCHY_SYSTEM_HPP
#define HVK_ECS_HIERARCHY_SYSTEM_HPP

#include <hvk/ecs/hvk_world.hpp>
#include <hvk/ecs/hvk_components.hpp>
#include <glm/glm.hpp>

namespace hvk {

/**
 * HierarchySystem - Propagates transforms through parent-child hierarchy
 *
 * Updates entities with ParentComponent to have world-space transforms
 * based on their parent's LocalToWorld matrix.
 *
 * Execution order:
 * 1. TransformSystem computes LocalToWorld for root entities (no parent)
 * 2. HierarchySystem propagates LocalToWorld down the hierarchy
 *    - Child's world transform = Parent's LocalToWorld * Child's local transform
 *
 * Design:
 * - Processes hierarchy level-by-level (breadth-first) for correctness
 * - Root entities (no ParentComponent) are unaffected
 * - Child entities have their TransformComponent treated as LOCAL space
 * - Final world-space matrix stored in LocalToWorld component
 *
 * Performance:
 * - Uses EnTT views for cache-friendly iteration
 * - Only processes entities with both TransformComponent AND ParentComponent
 * - Handles deep hierarchies efficiently
 */
class HierarchySystem : public ILogicSystem {
public:
    HierarchySystem() = default;
    ~HierarchySystem() override = default;

    /**
     * Update hierarchy transforms
     *
     * Propagates parent transforms to children recursively.
     * Assumes TransformSystem has already run this frame.
     *
     * @param world ECS world
     * @param deltaTime Time since last update (unused)
     */
    void update(entt::registry& registry, float deltaTime) override;

private:
    /**
     * Recursively update a child entity's world transform
     *
     * @param registry EnTT registry
     * @param entity Child entity to update
     * @param parentWorldMatrix Parent's world-space transform matrix
     */
    void updateChildRecursive(entt::registry& registry, entt::entity entity, const glm::mat4& parentWorldMatrix);

    /**
     * Compute local transform matrix from TransformComponent
     *
     * @param transform Transform component
     * @return 4x4 transformation matrix
     */
    static glm::mat4 computeLocalMatrix(const TransformComponent& transform);
};

} // namespace hvk

#endif // HVK_ECS_HIERARCHY_SYSTEM_HPP
