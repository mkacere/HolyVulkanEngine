/**
 * @file hvk_world.hpp
 * @brief ECS world container
 * @author Holy Vulkan Engine
 * @date 2025
 * Main ECS container managing entities, components, and systems.
 */

#ifndef HVK_ECS_WORLD_HPP
#define HVK_ECS_WORLD_HPP

#include <entt/entt.hpp>
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace hvk {

// Forward declarations
class ILogicSystem;

/**
 * World - Pure ECS world manager (No Vulkan dependencies)
 *
 * Design principles:
 * - Pure ECS: Only game logic, no rendering or Vulkan types
 * - Performance: Uses EnTT groups, observers, and cache-friendly iteration
 * - Simplicity: Clean API for entity/component management
 * - Extensibility: Plugin-based system architecture
 *
 * Separation of concerns:
 * - World: Pure game logic (transforms, hierarchy, physics, AI, etc.)
 * - RenderWorld: Rendering bridge (extracts ECS data → Vulkan)
 *
 * Usage:
 *   World world;
 *
 *   // Add logic systems
 *   world.addSystem<TransformSystem>();
 *   world.addSystem<HierarchySystem>();
 *
 *   // Create entities
 *   auto entity = world.createEntity("Player");
 *   world.add<TransformComponent>(entity, vec3(0, 0, 0));
 *
 *   // Update
 *   world.update(deltaTime);
 */
class World {
public:
    World();
    ~World();

    // Move-only
    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) noexcept = default;
    World& operator=(World&&) noexcept = default;

    // ========================================================================
    // Entity Management
    // ========================================================================

    /**
     * Create a new entity
     *
     * @param name Optional entity name (for debugging/lookup)
     * @return Entity handle
     */
    entt::entity createEntity(const std::string& name = "");

    /**
     * Destroy an entity and all its components
     *
     * @param entity Entity to destroy
     */
    void destroyEntity(entt::entity entity);

    /**
     * Check if entity is valid
     *
     * @param entity Entity to check
     * @return True if valid, false otherwise
     */
    bool isValid(entt::entity entity) const;

    /**
     * Find entity by name
     *
     * @param name Entity name
     * @return Entity handle (entt::null if not found)
     */
    entt::entity find(const std::string& name) const;

    /**
     * Get all entities with a specific name pattern
     *
     * @param namePattern Name pattern to match (exact match for now)
     * @return Vector of matching entities
     */
    std::vector<entt::entity> findAll(const std::string& namePattern) const;

    // ========================================================================
    // Component Management
    // ========================================================================

    /**
     * Add component to entity (or replace if exists)
     *
     * @param entity Entity to add component to
     * @param args Component constructor arguments
     * @return Reference to added component (for non-tag types)
     */
    template<typename T, typename... Args>
    decltype(auto) add(entt::entity entity, Args&&... args) {
        return registry_.emplace_or_replace<T>(entity, std::forward<Args>(args)...);
    }

    /**
     * Get component from entity
     *
     * @param entity Entity to get component from
     * @return Pointer to component (nullptr if not found)
     */
    template<typename T>
    T* get(entt::entity entity) {
        return registry_.try_get<T>(entity);
    }

    /**
     * Get component from entity (const)
     *
     * @param entity Entity to get component from
     * @return Pointer to component (nullptr if not found)
     */
    template<typename T>
    const T* get(entt::entity entity) const {
        return registry_.try_get<T>(entity);
    }

    /**
     * Check if entity has component
     *
     * @param entity Entity to check
     * @return True if entity has component, false otherwise
     */
    template<typename T>
    bool has(entt::entity entity) const {
        return registry_.all_of<T>(entity);
    }

    /**
     * Check if entity has all specified components
     *
     * @param entity Entity to check
     * @return True if entity has all components, false otherwise
     */
    template<typename... T>
    bool hasAll(entt::entity entity) const {
        return registry_.all_of<T...>(entity);
    }

    /**
     * Remove component from entity
     *
     * @param entity Entity to remove component from
     * @return Number of components removed (0 or 1)
     */
    template<typename T>
    size_t remove(entt::entity entity) {
        return registry_.remove<T>(entity);
    }

    // ========================================================================
    // System Management
    // ========================================================================

    /**
     * Add a logic system to the world
     *
     * Systems are updated in the order they were added.
     *
     * @param system System to add (takes ownership)
     */
    void addSystem(std::unique_ptr<ILogicSystem> system);

    /**
     * Add a logic system (construct in-place)
     *
     * @return Reference to the added system
     */
    template<typename T, typename... Args>
    T& addSystem(Args&&... args) {
        auto system = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = system.get();
        addSystem(std::move(system));
        return *ptr;
    }

    /**
     * Update all systems
     *
     * @param deltaTime Time since last update (seconds)
     */
    void update(float deltaTime);

    // ========================================================================
    // Advanced Access (EnTT Features)
    // ========================================================================

    /**
     * Get direct access to EnTT registry
     *
     * Use this for:
     * - Custom views and iteration
     * - Groups for cache-friendly iteration
     * - Observers for change detection
     * - Sorting for optimization
     *
     * Example - View iteration:
     *   auto view = world.registry().view<TransformComponent, VelocityComponent>();
     *   for (auto entity : view) {
     *       auto& transform = view.get<TransformComponent>(entity);
     *       auto& velocity = view.get<VelocityComponent>(entity);
     *       transform.position += velocity.value * deltaTime;
     *   }
     *
     * Example - Group (cache-friendly):
     *   auto group = world.registry().group<TransformComponent>(entt::get<MeshComponent>);
     *   for (auto entity : group) {
     *       auto& [transform, mesh] = group.get<TransformComponent, MeshComponent>(entity);
     *       // Extremely cache-friendly iteration
     *   }
     *
     * Example - Observer (change detection):
     *   entt::observer obs{world.registry(), entt::collector.update<TransformComponent>()};
     *   for (auto entity : obs) {
     *       // Only entities with modified TransformComponent
     *   }
     */
    entt::registry& registry() { return registry_; }
    const entt::registry& registry() const { return registry_; }

    /**
     * Clear all entities and components
     *
     * WARNING: Does not clear systems. Systems should be removed separately.
     */
    void clear();

    // Note: entityCount() and aliveEntityCount() methods removed for EnTT 3.15 compatibility
    // Use registry() to query entity counts directly if needed

private:
    // ECS registry (core of the world)
    entt::registry registry_;

    // Logic systems (executed in order during update)
    std::vector<std::unique_ptr<ILogicSystem>> systems_;
};

/**
 * ILogicSystem - Base interface for pure logic systems (no rendering)
 *
 * Logic systems operate only on ECS data, with no Vulkan dependencies.
 * Examples: TransformSystem, HierarchySystem, PhysicsSystem, AISystem
 *
 * Lifecycle:
 * 1. init(registry) - Called once when system is added
 * 2. update(registry, dt) - Called every frame during World::update()
 * 3. cleanup() - Called when system is removed or world is destroyed
 *
 * NOTE: Changed from World& to entt::registry& to allow safe adapter patterns
 * without dangerous casts. Systems only need registry access anyway.
 */
class ILogicSystem {
public:
    virtual ~ILogicSystem() = default;

    /**
     * Initialize system
     *
     * Called once when system is added to world.
     * Use this to create groups, observers, or allocate resources.
     *
     * @param registry ECS registry
     */
    virtual void init(entt::registry& registry) {}

    /**
     * Update system (game logic)
     *
     * Called every frame.
     * Use this for transforms, physics, AI, lifetime management, etc.
     *
     * @param registry ECS registry
     * @param deltaTime Time since last update (seconds)
     */
    virtual void update(entt::registry& registry, float deltaTime) = 0;

    /**
     * Cleanup system
     *
     * Called when system is removed or world is destroyed.
     * Use this to destroy resources.
     */
    virtual void cleanup() {}
};

} // namespace hvk

#endif // HVK_ECS_WORLD_HPP
