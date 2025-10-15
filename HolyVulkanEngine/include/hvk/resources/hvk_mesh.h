#ifndef HVK_MESH_H
#define HVK_MESH_H

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_gpu_resources.h>
#include <hvk/gfx/hvk_staging_uploader.h>
#include <hvk/gfx/hvk_cmd_list.hpp>
#include <hvk/resources/hvk_material.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace hvk {

/**
 * Vertex - Standard vertex format for PBR rendering
 *
 * Layout:
 *   Location 0: position (vec3)
 *   Location 1: normal (vec3)
 *   Location 2: uv (vec2)
 *   Location 3: color (vec4, optional)
 *   Location 4: tangent (vec4, xyz=tangent, w=bitangent sign)
 *
 * Total size: 64 bytes (well-aligned)
 */
struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;
    glm::vec4 color;      // Optional vertex colors (default white)
    glm::vec4 tangent;    // xyz = tangent, w = bitangent handedness (+1 or -1)

    // Default constructor
    Vertex()
        : position(0.0f)
        , normal(0.0f, 0.0f, 1.0f)
        , uv(0.0f)
        , color(1.0f)
        , tangent(1.0f, 0.0f, 0.0f, 1.0f)
    {}

    Vertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& uv_)
        : position(pos)
        , normal(norm)
        , uv(uv_)
        , color(1.0f)
        , tangent(1.0f, 0.0f, 0.0f, 1.0f)
    {}

    // Get vertex input binding description
    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding;
    }

    // Get vertex input attribute descriptions
    static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributes(5);

        // Location 0: position
        attributes[0].binding = 0;
        attributes[0].location = 0;
        attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[0].offset = offsetof(Vertex, position);

        // Location 1: normal
        attributes[1].binding = 0;
        attributes[1].location = 1;
        attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributes[1].offset = offsetof(Vertex, normal);

        // Location 2: uv
        attributes[2].binding = 0;
        attributes[2].location = 2;
        attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
        attributes[2].offset = offsetof(Vertex, uv);

        // Location 3: color
        attributes[3].binding = 0;
        attributes[3].location = 3;
        attributes[3].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[3].offset = offsetof(Vertex, color);

        // Location 4: tangent
        attributes[4].binding = 0;
        attributes[4].location = 4;
        attributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributes[4].offset = offsetof(Vertex, tangent);

        return attributes;
    }

    bool operator==(const Vertex& other) const {
        return position == other.position &&
               normal == other.normal &&
               uv == other.uv &&
               color == other.color &&
               tangent == other.tangent;
    }
};

// Validate size (should be 64 bytes for good alignment)
static_assert(sizeof(Vertex) == 64, "Vertex should be 64 bytes");

/**
 * AABB - Axis-aligned bounding box
 */
struct AABB {
    glm::vec3 min{FLT_MAX};
    glm::vec3 max{-FLT_MAX};

    AABB() = default;

    AABB(const glm::vec3& min_, const glm::vec3& max_)
        : min(min_), max(max_)
    {}

    void expand(const glm::vec3& point) {
        min = glm::min(min, point);
        max = glm::max(max, point);
    }

    void expand(const AABB& other) {
        min = glm::min(min, other.min);
        max = glm::max(max, other.max);
    }

    glm::vec3 center() const {
        return (min + max) * 0.5f;
    }

    glm::vec3 extents() const {
        return (max - min) * 0.5f;
    }

    bool isValid() const {
        return min.x <= max.x && min.y <= max.y && min.z <= max.z;
    }
};

/**
 * Mesh - Single drawable mesh with vertex/index buffers
 *
 * Design notes:
 * - Owns vertex and index buffers
 * - References a Material (non-owning)
 * - Stores local AABB for culling
 * - Provides draw() method for command recording
 */
class Mesh {
public:
    Mesh() = default;
    ~Mesh() = default;

    // Move-only
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&&) noexcept = default;
    Mesh& operator=(Mesh&&) noexcept = default;

    /**
     * Create mesh from vertex and index data
     *
     * @param device Device reference
     * @param uploader Staging uploader for GPU upload
     * @param vertices Vertex data
     * @param indices Index data (uint32_t)
     * @param material Material reference (can be nullptr)
     * @param debugName Debug name for buffers
     */
    void create(
        const Device& device,
        StagingUploader& uploader,
        const std::vector<Vertex>& vertices,
        const std::vector<uint32_t>& indices,
        Material* material = nullptr,
        const std::string& debugName = "mesh"
    );

    /**
     * Draw the mesh
     *
     * @param cmd Command list
     * @param bindMaterial If true, binds the material descriptor set (Set 1)
     * @param materialSetLayout Pipeline layout for material binding
     */
    void draw(CmdList& cmd, bool bindMaterial = false, VkPipelineLayout materialSetLayout = VK_NULL_HANDLE) const;

    // Accessors
    uint32_t vertexCount() const { return vertexCount_; }
    uint32_t indexCount() const { return indexCount_; }
    Material* material() const { return material_; }
    const AABB& bounds() const { return bounds_; }

    VkBuffer vertexBuffer() const { return vertexBuffer_.handle(); }
    VkBuffer indexBuffer() const { return indexBuffer_.handle(); }

    // Setters
    void setMaterial(Material* mat) { material_ = mat; }
    void setName(const std::string& name) { name_ = name; }
    const std::string& name() const { return name_; }

private:
    void computeBounds(const std::vector<Vertex>& vertices);

private:
    GpuBuffer vertexBuffer_;
    GpuBuffer indexBuffer_;
    uint32_t  vertexCount_ = 0;
    uint32_t  indexCount_ = 0;
    Material* material_ = nullptr;  // Non-owning
    AABB      bounds_;
    std::string name_;
};

/**
 * MeshBuilder - Helper for building meshes
 *
 * Usage:
 *   std::vector<Vertex> verts = { ... };
 *   std::vector<uint32_t> inds = { ... };
 *   Mesh mesh = MeshBuilder()
 *       .withVertices(verts)
 *       .withIndices(inds)
 *       .withMaterial(material)
 *       .build(device, uploader);
 */
class MeshBuilder {
public:
    MeshBuilder() = default;

    MeshBuilder& withVertices(const std::vector<Vertex>& verts) {
        vertices_ = verts;
        return *this;
    }

    MeshBuilder& withIndices(const std::vector<uint32_t>& inds) {
        indices_ = inds;
        return *this;
    }

    MeshBuilder& withMaterial(Material* mat) {
        material_ = mat;
        return *this;
    }

    MeshBuilder& withName(const std::string& name) {
        name_ = name;
        return *this;
    }

    Mesh build(const Device& device, StagingUploader& uploader);

private:
    std::vector<Vertex> vertices_;
    std::vector<uint32_t> indices_;
    Material* material_ = nullptr;
    std::string name_;
};

} // namespace hvk

#endif // HVK_MESH_H
