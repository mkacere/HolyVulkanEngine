#ifndef HVK_ECS_RESOURCE_MANAGER_HPP
#define HVK_ECS_RESOURCE_MANAGER_HPP

#include <hvk/resources/hvk_model.h>
#include <hvk/resources/hvk_texture.h>
#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_staging_uploader.h>
#include <hvk/gfx/hvk_sampler_cache.h>

#include <vector>
#include <string>
#include <unordered_map>
#include <optional>
#include <memory>

namespace hvk {

/**
 * ResourceManager - Central manager for all GPU resources
 *
 * Design principles:
 * - Pool pattern: Resources are stored in contiguous vectors with handle-based access
 * - Ownership: ResourceManager owns all resources (RAII cleanup)
 * - Thread-safe: Resources are immutable after creation (read-only access)
 * - Caching: Named resources are cached to avoid duplicate loads
 * - Handle-based: Components store handles (indices), not pointers
 *
 * Usage:
 *   ResourceManager resources(device, uploader, samplerCache);
 *   uint32_t modelHandle = resources.loadModel("assets/models/foo.glb");
 *   Model* model = resources.getModel(modelHandle);
 */
class ResourceManager {
public:
    // Handle type (just an index into the resource pool)
    using Handle = uint32_t;
    static constexpr Handle INVALID_HANDLE = UINT32_MAX;

    /**
     * Constructor
     *
     * @param device Vulkan device wrapper
     * @param uploader Staging uploader for GPU transfers
     * @param samplerCache Sampler cache for texture samplers
     * @param descriptorAllocator Descriptor allocator for materials
     * @param materialLayout Material descriptor set layout (Set 1)
     */
    ResourceManager(
        const Device& device,
        StagingUploader& uploader,
        SamplerCache& samplerCache,
        DescriptorAllocator& descriptorAllocator,
        const DescriptorSetLayout& materialLayout
    );

    ~ResourceManager();

    // Move-only
    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
    ResourceManager(ResourceManager&&) noexcept = default;
    ResourceManager& operator=(ResourceManager&&) noexcept = default;

    // ========================================================================
    // Model Loading
    // ========================================================================

    /**
     * Load a GLTF model from file (with caching)
     *
     * If the model has already been loaded (same path), returns existing handle.
     * Otherwise, loads the model and caches it.
     *
     * @param path Path to .glb or .gltf file
     * @param generateMipmaps Generate mipmaps for textures?
     * @param flipTextureY Flip texture Y coordinates?
     * @return Handle to the loaded model (INVALID_HANDLE on failure)
     */
    Handle loadModel(
        const std::string& path,
        bool generateMipmaps = true,
        bool flipTextureY = false
    );

    /**
     * Add a pre-loaded model to the pool
     *
     * @param model Model to add (takes ownership)
     * @param name Optional name for caching
     * @return Handle to the added model
     */
    Handle addModel(Model&& model, const std::string& name = "");

    /**
     * Get model by handle (read-only)
     *
     * @param handle Model handle
     * @return Pointer to model (nullptr if invalid handle)
     */
    const Model* getModel(Handle handle) const;

    /**
     * Get model by handle (mutable)
     *
     * @param handle Model handle
     * @return Pointer to model (nullptr if invalid handle)
     */
    Model* getModel(Handle handle);

    /**
     * Find model handle by name
     *
     * @param name Model name (file path or custom name)
     * @return Handle to model (INVALID_HANDLE if not found)
     */
    Handle findModel(const std::string& name) const;

    /**
     * Unload a model by handle
     *
     * Frees GPU resources and removes from pool.
     * WARNING: Invalidates all handles >= this handle!
     * Use with caution - better to keep resources loaded.
     *
     * @param handle Model handle
     */
    void unloadModel(Handle handle);

    /**
     * Unload all models
     */
    void clearModels();

    /**
     * Get number of loaded models
     */
    size_t modelCount() const { return models_.size(); }

    // ========================================================================
    // Accessors
    // ========================================================================

    const Device& device() const { return *device_; }
    const DescriptorSetLayout* materialDescriptorLayout() const { return materialLayout_; }

    // ========================================================================
    // Future: Texture, Material, Shader management
    // ========================================================================

    // TODO: Add texture loading/caching
    // TODO: Add material creation
    // TODO: Add shader module caching

private:
    // Dependencies
    const Device* device_;
    StagingUploader* uploader_;
    SamplerCache* samplerCache_;
    DescriptorAllocator* descriptorAllocator_;
    const DescriptorSetLayout* materialLayout_;

    // Resource pools
    std::vector<Model> models_;

    // Name -> Handle lookup (for caching)
    std::unordered_map<std::string, Handle> modelCache_;
};

} // namespace hvk

#endif // HVK_ECS_RESOURCE_MANAGER_HPP
