#ifndef HVK_ECS_TRANSFORM_SYSTEM_HPP
#define HVK_ECS_TRANSFORM_SYSTEM_HPP

#include <hvk/ecs/hvk_world.hpp>
#include <entt/entt.hpp>
#include <memory>

namespace hvk {

/**
 * TransformSystem - Computes transform matrices from TransformComponent
 *
 * Responsibilities:
 * - Compute LocalToWorld matrices from TransformComponent (pos/rot/scale)
 * - Compute normal matrices for lighting calculations
 * - Cache-friendly iteration using EnTT views
 *
 * Component requirements:
 * - TransformComponent (input - pure data: pos, rot, scale)
 * - LocalToWorld (output - computed matrix data)
 *
 * Performance:
 * - Uses EnTT view for cache-friendly iteration
 * - Processes all transforms each frame (still very fast due to cache locality)
 * - Future: Could add change tracking via entt::sigh for minimal overhead
 */
class TransformSystem : public ILogicSystem {
public:
    TransformSystem() = default;
    ~TransformSystem() override = default;

    void update(entt::registry& registry, float deltaTime) override;
};

} // namespace hvk

#endif // HVK_ECS_TRANSFORM_SYSTEM_HPP
