/**
 * @file hvk_scene.hpp
 * @brief ECS scene graph management
 * @author Holy Vulkan Engine
 * @date 2025
 * Manages hierarchical scene graphs with transform propagation.
 */

#ifndef HVK_ECS_SCENE_HPP
#define HVK_ECS_SCENE_HPP

#include <hvk/ecs/hvk_components.hpp>
#include <hvk/ecs/hvk_resource_manager.hpp>
#include <entt/entt.hpp>

#include <string>
#include <memory>
#include <vector>

namespace hvk {

// Forward declarations
class ISystem;
class GraphicsPipelineCache;
class GlobalDescriptorSet;

/**
 * Scene - High-level ECS scene manager with easy-to-use API
 *
 * Design principles:
 * - Wraps EnTT registry with convenient methods
 * - Integrates ResourceManager for asset management
 * - System-based architecture for update/render
 * - Default options with full control available
 *
 * Usage:
 *   Scene scene(device, uploader, samplerCache);
 *
 *   // Easy model loading
 *   auto entity = scene.loadModel("assets/models/foo.glb");
 *
 *   // Easy entity creation
 *   auto light = scene.createEntity("Sun");
 *   scene.addComponent<DirectionalLightComponent>(light, glm::vec3(1.0f), 1.0f);
 *
 *   // Access components
 *   auto* transform = scene.getComponent<TransformComponent>(entity);
 *   transform->position.y += 1.0f;
 */
class Scene {
public:
    /**
     * Constructor
     *
     * @param device Vulkan device wrapper
     * @param uploader Staging uploader for GPU transfers
     * @param samplerCache Sampler cache for texture samplers
     * @param descriptorAllocator Descriptor allocator for materials
     * @param materialLayout Material descriptor set layout (Set 1)
     * @param pipelineCache Graphics pipeline cache for systems
     * @param globalDescLayout Global descriptor set layout (Set 0)
     * @param globalDescSet Global descriptor set (Set 0) for rendering
     */
    Scene(
        const Device& device,
        StagingUploader& uploader,
        SamplerCache& samplerCache,
        DescriptorAllocator& descriptorAllocator,
        const DescriptorSetLayout& materialLayout,
        GraphicsPipelineCache* pipelineCache,
        const DescriptorSetLayout* globalDescLayout,
        GlobalDescriptorSet* globalDescSet
    );

    ~Scene();

    // Move-only
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;

    // ========================================================================
    // Entity Management (Easy API)
    // ========================================================================

    /**
     * Create a new entity
     *
     * @param name Optional entity name
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
    entt::entity findEntity(const std::string& name) const;

    // ========================================================================
    // Component Management (Easy API)
    // ========================================================================

    /**
     * Add component to entity
     *
     * @param entity Entity to add component to
     * @param args Component constructor arguments
     * @return Reference to added component (for non-empty types)
     *
     * Note: For tag components (empty structs), use emplace_or_replace which returns void
     */
    template<typename T, typename... Args>
    decltype(auto) addComponent(entt::entity entity, Args&&... args) {
        return registry_.emplace_or_replace<T>(entity, std::forward<Args>(args)...);
    }

    /**
     * Get component from entity
     *
     * @param entity Entity to get component from
     * @return Pointer to component (nullptr if not found)
     */
    template<typename T>
    T* getComponent(entt::entity entity) {
        return registry_.try_get<T>(entity);
    }

    /**
     * Get component from entity (const)
     *
     * @param entity Entity to get component from
     * @return Pointer to component (nullptr if not found)
     */
    template<typename T>
    const T* getComponent(entt::entity entity) const {
        return registry_.try_get<T>(entity);
    }

    /**
     * Check if entity has component
     *
     * @param entity Entity to check
     * @return True if entity has component, false otherwise
     */
    template<typename T>
    bool hasComponent(entt::entity entity) const {
        return registry_.all_of<T>(entity);
    }

    /**
     * Remove component from entity
     *
     * @param entity Entity to remove component from
     */
    template<typename T>
    void removeComponent(entt::entity entity) {
        registry_.remove<T>(entity);
    }

    // ========================================================================
    // Model Loading (Easy API)
    // ========================================================================

    /**
     * Load a model and create an entity for it
     *
     * Creates an entity with:
     * - TransformComponent (at specified position/rotation/scale)
     * - MeshComponent (referencing loaded model)
     * - NameComponent (model filename)
     * - ActiveTag
     *
     * @param path Path to .glb or .gltf file
     * @param position Initial position (default: origin)
     * @param rotation Initial rotation (default: identity)
     * @param scale Initial scale (default: 1,1,1)
     * @param generateMipmaps Generate mipmaps for textures? (default: true)
     * @return Entity handle (entt::null on failure)
     */
    entt::entity loadModel(
        const std::string& path,
        const glm::vec3& position = glm::vec3(0.0f),
        const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        const glm::vec3& scale = glm::vec3(1.0f),
        bool generateMipmaps = true
    );

    // ========================================================================
    // Spawn Helpers (Easy API) - High-level entity creation
    // ========================================================================

    /**
     * Spawn a model entity (simplified API)
     *
     * Same as loadModel() but with simplified parameters.
     *
     * @param path Path to .glb or .gltf file
     * @param position Initial position
     * @return Entity handle (entt::null on failure)
     */
    entt::entity spawnModel(const std::string& path, const glm::vec3& position = glm::vec3(0.0f));

    /**
     * Spawn a point light entity
     *
     * Creates an entity with:
     * - TransformComponent (at specified position)
     * - PointLightComponent
     * - NameComponent
     *
     * @param position Light position
     * @param color Light color (default: white)
     * @param intensity Light intensity (default: 1.0)
     * @param radius Attenuation radius (default: 10.0)
     * @return Entity handle
     */
    entt::entity spawnPointLight(
        const glm::vec3& position,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f,
        float radius = 10.0f
    );

    /**
     * Spawn a directional light entity (like sun)
     *
     * Creates an entity with:
     * - TransformComponent (rotation determines direction)
     * - DirectionalLightComponent
     * - NameComponent
     *
     * @param direction Light direction
     * @param color Light color (default: white)
     * @param intensity Light intensity (default: 1.0)
     * @return Entity handle
     */
    entt::entity spawnDirectionalLight(
        const glm::vec3& direction,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f
    );

    /**
     * Spawn a camera entity
     *
     * Creates an entity with:
     * - TransformComponent (at position, looking at target)
     * - CameraComponent (perspective)
     * - NameComponent
     *
     * @param position Camera position
     * @param target Look-at target (default: origin)
     * @param fovYDegrees Vertical FOV (default: 60)
     * @return Entity handle
     */
    entt::entity spawnCamera(
        const glm::vec3& position,
        const glm::vec3& target = glm::vec3(0.0f),
        float fovYDegrees = 60.0f
    );

    /**
     * Load a model with automatic centering
     *
     * Same as loadModel(), but centers the model at the specified position
     * by applying a centering transform based on world-space bounds.
     *
     * @param path Path to .glb or .gltf file
     * @param position Center position (default: origin)
     * @param rotation Initial rotation (default: identity)
     * @param scale Initial scale (default: 1,1,1)
     * @param generateMipmaps Generate mipmaps for textures? (default: true)
     * @return Entity handle (entt::null on failure)
     */
    entt::entity loadModelCentered(
        const std::string& path,
        const glm::vec3& position = glm::vec3(0.0f),
        const glm::quat& rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
        const glm::vec3& scale = glm::vec3(1.0f),
        bool generateMipmaps = true
    );

    // ========================================================================
    // Camera Helpers (Easy API)
    // ========================================================================

    /**
     * Create a perspective camera entity
     *
     * @param position Camera position
     * @param target Look-at target
     * @param fovYDegrees Vertical field of view in degrees (default: 45)
     * @param aspectRatio Aspect ratio (default: 16:9)
     * @param nearPlane Near clipping plane (default: 0.1)
     * @param farPlane Far clipping plane (default: 1000)
     * @return Entity handle
     */
    entt::entity createPerspectiveCamera(
        const glm::vec3& position,
        const glm::vec3& target = glm::vec3(0.0f),
        float fovYDegrees = 45.0f,
        float aspectRatio = 16.0f / 9.0f,
        float nearPlane = 0.1f,
        float farPlane = 1000.0f
    );

    // ========================================================================
    // Hierarchy Helpers (Easy API)
    // ========================================================================

    /**
     * Set parent-child relationship
     *
     * Makes 'child' a child of 'parent'. The child's TransformComponent
     * will be treated as LOCAL space relative to parent.
     * HierarchySystem will compute world-space transform.
     *
     * Automatically maintains ChildrenComponent on parent.
     *
     * @param child Child entity
     * @param parent Parent entity
     */
    void setParent(entt::entity child, entt::entity parent);

    /**
     * Remove parent-child relationship
     *
     * Makes 'child' a root entity again (no parent).
     * Child's current LOCAL transform is preserved.
     *
     * @param child Child entity to detach
     */
    void removeParent(entt::entity child);

    /**
     * Get all children of an entity
     *
     * @param parent Parent entity
     * @return Vector of child entities (empty if no children)
     */
    std::vector<entt::entity> getChildren(entt::entity parent) const;

    /**
     * Get parent of an entity
     *
     * @param child Child entity
     * @return Parent entity (entt::null if no parent)
     */
    entt::entity getParent(entt::entity child) const;

    /**
     * Get the active camera entity
     *
     * @return Active camera entity (entt::null if none)
     */
    entt::entity getActiveCamera() const;

    /**
     * Set the active camera
     *
     * @param entity Camera entity to activate
     */
    void setActiveCamera(entt::entity entity);

    // ========================================================================
    // Light Helpers (Easy API)
    // ========================================================================

    /**
     * Create a directional light entity (like sun)
     *
     * @param direction Light direction
     * @param color Light color (default: white)
     * @param intensity Light intensity (default: 1.0)
     * @return Entity handle
     */
    entt::entity createDirectionalLight(
        const glm::vec3& direction,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f
    );

    /**
     * Create a point light entity
     *
     * @param position Light position
     * @param color Light color (default: white)
     * @param intensity Light intensity (default: 1.0)
     * @param radius Attenuation radius (default: 10.0)
     * @return Entity handle
     */
    entt::entity createPointLight(
        const glm::vec3& position,
        const glm::vec3& color = glm::vec3(1.0f),
        float intensity = 1.0f,
        float radius = 10.0f
    );

    // ========================================================================
    // Advanced Access (Full Control)
    // ========================================================================

    /**
     * Get direct access to EnTT registry
     *
     * Use this for advanced queries, views, and iteration.
     *
     * Example:
     *   auto view = scene.registry().view<TransformComponent, MeshComponent>();
     *   for (auto entity : view) {
     *       auto& transform = view.get<TransformComponent>(entity);
     *       // ...
     *   }
     */
    entt::registry& registry() { return registry_; }
    const entt::registry& registry() const { return registry_; }

    /**
     * Get resource manager
     */
    ResourceManager& resources() { return resources_; }
    const ResourceManager& resources() const { return resources_; }

    /**
     * Get pipeline cache (may be null)
     */
    GraphicsPipelineCache* pipelineCache() { return pipelineCache_; }
    const GraphicsPipelineCache* pipelineCache() const { return pipelineCache_; }

    /**
     * Get global descriptor set (may be null)
     */
    GlobalDescriptorSet* globalDescriptorSet() { return globalDescSet_; }
    const GlobalDescriptorSet* globalDescriptorSet() const { return globalDescSet_; }

    /**
     * Get global descriptor set layout (may be null)
     */
    const DescriptorSetLayout* globalDescriptorLayout() const { return globalDescLayout_; }

    /**
     * Get current frame index
     */
    uint32_t frameIndex() const { return frameIndex_; }

    // ========================================================================
    // System Management
    // ========================================================================

    /**
     * Add a system to the scene
     *
     * Systems are updated/rendered in the order they were added.
     *
     * @param system System to add (takes ownership)
     */
    void addSystem(std::unique_ptr<ISystem> system);

    /**
     * Update all systems
     *
     * @param deltaTime Time since last update (seconds)
     */
    void update(float deltaTime);

    /**
     * Render all systems
     *
     * @param cmd Command list
     * @param frameIndex Current frame index for descriptor sets
     */
    void render(CmdList& cmd, uint32_t frameIndex = 0);

private:
    // ECS registry
    entt::registry registry_;

    // Resource management
    ResourceManager resources_;

    // Systems (executed in order)
    std::vector<std::unique_ptr<ISystem>> systems_;

    // Caches and descriptor sets (not owned, may be null)
    GraphicsPipelineCache* pipelineCache_ = nullptr;
    const DescriptorSetLayout* globalDescLayout_ = nullptr;
    GlobalDescriptorSet* globalDescSet_ = nullptr;

    // Frame tracking
    uint32_t frameIndex_ = 0;
};

} // namespace hvk

#endif // HVK_ECS_SCENE_HPP
