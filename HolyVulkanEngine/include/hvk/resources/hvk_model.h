#ifndef HVK_MODEL_H
#define HVK_MODEL_H

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_descriptors.h>
#include <hvk/gfx/hvk_staging_uploader.h>
#include <hvk/gfx/hvk_sampler_cache.h>
#include <hvk/gfx/hvk_cmd_list.hpp>
#include <hvk/resources/hvk_texture.h>
#include <hvk/resources/hvk_material.h>
#include <hvk/resources/hvk_mesh.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <memory>

namespace hvk {

/**
 * Node - Scene graph node with transform hierarchy
 *
 * Design notes:
 * - Supports local and world transforms
 * - Can reference a Mesh (optional)
 * - Supports parent-child hierarchy
 * - World transform is computed by traversing hierarchy
 */
struct Node {
    std::string name;
    glm::mat4 localTransform{1.0f};   // Local transform (relative to parent)
    glm::mat4 worldTransform{1.0f};   // World transform (computed from hierarchy)

    int32_t meshIndex = -1;            // Index into Model::meshes_ (-1 = no mesh)
    int32_t parentIndex = -1;          // Index into Model::nodes_ (-1 = root)
    std::vector<int32_t> children;     // Indices of child nodes

    Node() = default;

    Node(const std::string& name_, const glm::mat4& transform = glm::mat4(1.0f))
        : name(name_), localTransform(transform), worldTransform(transform)
    {}
};

/**
 * Model - Container for meshes, materials, textures, and scene graph
 *
 * Design notes:
 * - Owns all resources (textures, materials, meshes, nodes)
 * - Provides draw() method that traverses scene graph
 * - Supports GLTF-style hierarchy (nodes with transforms)
 * - Materials reference textures (non-owning)
 * - Meshes reference materials (non-owning)
 * - Thread-safe after construction (read-only)
 *
 * Usage:
 *   Model model = GltfLoader::loadFromFile(...);
 *   model.updateTransforms();  // Compute world transforms
 *   model.draw(cmd, pipelineLayout, globalDescSet);
 */
class Model {
public:
    Model() = default;
    ~Model() = default;

    // Move-only
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) noexcept = default;
    Model& operator=(Model&&) noexcept = default;

    // --- Resource Management ---

    /**
     * Reserve space for resources (prevents vector reallocation)
     */
    void reserveTextures(size_t count) { textures_.reserve(count); }
    void reserveMaterials(size_t count) { materials_.reserve(count); }
    void reserveMeshes(size_t count) { meshes_.reserve(count); }
    void reserveNodes(size_t count) { nodes_.reserve(count); }

    /**
     * Add a texture to the model (takes ownership)
     */
    size_t addTexture(Texture&& texture) {
        textures_.push_back(std::move(texture));
        return textures_.size() - 1;
    }

    /**
     * Add a material to the model (takes ownership)
     */
    size_t addMaterial(Material&& material) {
        materials_.push_back(std::move(material));
        return materials_.size() - 1;
    }

    /**
     * Add a mesh to the model (takes ownership)
     */
    size_t addMesh(Mesh&& mesh) {
        meshes_.push_back(std::move(mesh));
        bounds_.expand(meshes_.back().bounds());
        return meshes_.size() - 1;
    }

    /**
     * Add a node to the scene graph
     */
    size_t addNode(const Node& node) {
        nodes_.push_back(node);
        return nodes_.size() - 1;
    }

    /**
     * Set the root node index
     */
    void setRootNode(int32_t nodeIndex) {
        rootNodeIndex_ = nodeIndex;
    }

    // --- Transform Updates ---

    /**
     * Update all world transforms by traversing the scene graph
     *
     * Call this after modifying node local transforms
     */
    void updateTransforms();

    /**
     * Update world transforms starting from a specific node
     */
    void updateTransforms(int32_t nodeIndex, const glm::mat4& parentTransform = glm::mat4(1.0f));

    // --- Drawing ---

    /**
     * Draw the entire model (traverses scene graph)
     *
     * @param cmd Command list
     * @param pipelineLayout Pipeline layout (for push constants and descriptor sets)
     * @param globalDescSet Global descriptor set (Set 0: Scene+Camera+Lights)
     * @param modelTransform Root model transform (applied to all nodes)
     */
    void draw(
        CmdList& cmd,
        VkPipelineLayout pipelineLayout,
        VkDescriptorSet globalDescSet,
        const glm::mat4& modelTransform = glm::mat4(1.0f)
    ) const;

    /**
     * Draw a specific node (and its children)
     */
    void drawNode(
        int32_t nodeIndex,
        CmdList& cmd,
        VkPipelineLayout pipelineLayout,
        VkDescriptorSet globalDescSet,
        const glm::mat4& modelTransform
    ) const;

    // --- Accessors ---

    size_t textureCount() const { return textures_.size(); }
    size_t materialCount() const { return materials_.size(); }
    size_t meshCount() const { return meshes_.size(); }
    size_t nodeCount() const { return nodes_.size(); }

    Texture* texture(size_t index) { return index < textures_.size() ? &textures_[index] : nullptr; }
    Material* material(size_t index) { return index < materials_.size() ? &materials_[index] : nullptr; }
    Mesh* mesh(size_t index) { return index < meshes_.size() ? &meshes_[index] : nullptr; }
    Node* node(size_t index) { return index < nodes_.size() ? &nodes_[index] : nullptr; }

    const Texture* texture(size_t index) const { return index < textures_.size() ? &textures_[index] : nullptr; }
    const Material* material(size_t index) const { return index < materials_.size() ? &materials_[index] : nullptr; }
    const Mesh* mesh(size_t index) const { return index < meshes_.size() ? &meshes_[index] : nullptr; }
    const Node* node(size_t index) const { return index < nodes_.size() ? &nodes_[index] : nullptr; }

    const AABB& bounds() const { return bounds_; }
    int32_t rootNodeIndex() const { return rootNodeIndex_; }

    void setName(const std::string& name) { name_ = name; }
    const std::string& name() const { return name_; }

    // --- Default Textures ---

    /**
     * Create default textures (white, normal, metallic-roughness)
     *
     * These are used for materials that don't have all texture maps
     */
    void createDefaultTextures(
        const Device& device,
        StagingUploader& uploader,
        SamplerCache& samplerCache
    );

    Texture* defaultWhiteTexture() { return defaultWhite_ ? &(*defaultWhite_) : nullptr; }
    Texture* defaultNormalTexture() { return defaultNormal_ ? &(*defaultNormal_) : nullptr; }
    Texture* defaultMetallicRoughnessTexture() { return defaultMetallicRoughness_ ? &(*defaultMetallicRoughness_) : nullptr; }

private:
    // Resources (owned)
    std::vector<Texture> textures_;
    std::vector<Material> materials_;
    std::vector<Mesh> meshes_;
    std::vector<Node> nodes_;

    // Scene graph
    int32_t rootNodeIndex_ = -1;

    // Bounds
    AABB bounds_;

    // Metadata
    std::string name_;

    // Default textures (optional, for materials without textures)
    std::optional<Texture> defaultWhite_;
    std::optional<Texture> defaultNormal_;
    std::optional<Texture> defaultMetallicRoughness_;
};

} // namespace hvk

#endif // HVK_MODEL_H
