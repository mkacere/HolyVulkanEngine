#include <hvk/ecs/hvk_resource_manager.hpp>
#include <hvk/resources/loaders/hvk_gltf_loader.h>
#include <stdexcept>
#include <algorithm>

namespace hvk {

ResourceManager::ResourceManager(
    const Device& device,
    StagingUploader& uploader,
    SamplerCache& samplerCache,
    DescriptorAllocator& descriptorAllocator,
    const DescriptorSetLayout& materialLayout
)
    : device_(&device)
    , uploader_(&uploader)
    , samplerCache_(&samplerCache)
    , descriptorAllocator_(&descriptorAllocator)
    , materialLayout_(&materialLayout)
{
}

ResourceManager::~ResourceManager() {
    clearModels();
}

// ============================================================================
// Model Loading
// ============================================================================

ResourceManager::Handle ResourceManager::loadModel(
    const std::string& path,
    bool generateMipmaps,
    bool flipTextureY
) {
    // Check cache first
    auto it = modelCache_.find(path);
    if (it != modelCache_.end()) {
        return it->second;
    }

    // Load model using GltfLoader
    GltfLoaderOptions options;
    options.generateMipmaps = generateMipmaps;
    options.flipTextureY = flipTextureY;
    options.loadMaterials = true;
    options.loadTextures = true;

    Model model = GltfLoader::loadFromFile(
        *device_,
        *uploader_,
        *descriptorAllocator_,
        *materialLayout_,
        *samplerCache_,
        path,
        options
    );

    // Check if model loaded successfully
    if (model.meshCount() == 0) {
        // Failed to load
        return INVALID_HANDLE;
    }

    // Set model name to path
    model.setName(path);

    // Add to pool
    return addModel(std::move(model), path);
}

ResourceManager::Handle ResourceManager::addModel(Model&& model, const std::string& name) {
    Handle handle = static_cast<Handle>(models_.size());
    models_.push_back(std::move(model));

    // Cache by name if provided
    if (!name.empty()) {
        modelCache_[name] = handle;
    }

    return handle;
}

const Model* ResourceManager::getModel(Handle handle) const {
    if (handle >= models_.size()) {
        return nullptr;
    }
    return &models_[handle];
}

Model* ResourceManager::getModel(Handle handle) {
    if (handle >= models_.size()) {
        return nullptr;
    }
    return &models_[handle];
}

ResourceManager::Handle ResourceManager::findModel(const std::string& name) const {
    auto it = modelCache_.find(name);
    if (it != modelCache_.end()) {
        return it->second;
    }
    return INVALID_HANDLE;
}

void ResourceManager::unloadModel(Handle handle) {
    if (handle >= models_.size()) {
        return;
    }

    // Remove from cache (find by value)
    for (auto it = modelCache_.begin(); it != modelCache_.end(); ) {
        if (it->second == handle) {
            it = modelCache_.erase(it);
        } else {
            ++it;
        }
    }

    // Remove from pool (swap with last element)
    if (handle < models_.size() - 1) {
        models_[handle] = std::move(models_.back());

        // Update cache for swapped element
        for (auto& [name, h] : modelCache_) {
            if (h == models_.size() - 1) {
                h = handle;
            }
        }
    }

    models_.pop_back();
}

void ResourceManager::clearModels() {
    models_.clear();
    modelCache_.clear();
}

} // namespace hvk
