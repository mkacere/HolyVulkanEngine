#ifndef HVK_ECS_SYSTEM_HPP
#define HVK_ECS_SYSTEM_HPP

#include <entt/entt.hpp>

namespace hvk {

// Forward declarations
class CmdList;
class Scene;

/**
 * ISystem - Base interface for all ECS systems
 *
 * Systems operate on entities with specific component combinations.
 * They encapsulate behavior and logic for the ECS architecture.
 *
 * Design:
 * - Systems don't own data (components do)
 * - Systems are added to Scene and executed in order
 * - Systems can query the registry via Scene reference
 *
 * Lifecycle:
 * 1. init() - Called once when system is added to scene
 * 2. update(dt) - Called every frame (logic, transforms, physics, etc.)
 * 3. render(cmd) - Called during render pass (draw calls, GPU commands)
 */
class ISystem {
public:
    virtual ~ISystem() = default;

    /**
     * Initialize system
     *
     * Called once when system is added to scene.
     * Use this to create pipelines, allocate resources, etc.
     *
     * @param scene Parent scene
     */
    virtual void init(Scene& scene) {}

    /**
     * Update system (logic, transforms, physics, etc.)
     *
     * Called every frame before rendering.
     *
     * @param scene Parent scene
     * @param deltaTime Time since last update (seconds)
     */
    virtual void update(Scene& scene, float deltaTime) {}

    /**
     * Render system (draw calls, GPU commands)
     *
     * Called during render pass.
     *
     * @param scene Parent scene
     * @param cmd Command list
     */
    virtual void render(Scene& scene, CmdList& cmd) {}

    /**
     * Cleanup system
     *
     * Called when system is removed or scene is destroyed.
     * Use this to destroy Vulkan resources.
     */
    virtual void cleanup() {}
};

} // namespace hvk

#endif // HVK_ECS_SYSTEM_HPP
