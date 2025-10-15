#include <hvk/resources/hvk_mesh.h>
#include <hvk/gfx/hvk_utils.hpp>

namespace hvk {

void Mesh::create(
    const Device& device,
    StagingUploader& uploader,
    const std::vector<Vertex>& vertices,
    const std::vector<uint32_t>& indices,
    Material* material,
    const std::string& debugName
) {
    vertexCount_ = static_cast<uint32_t>(vertices.size());
    indexCount_ = static_cast<uint32_t>(indices.size());
    material_ = material;
    name_ = debugName;

    if (vertexCount_ == 0) {
        throw std::runtime_error("Mesh::create: vertex count is 0");
    }

    // Compute bounding box
    computeBounds(vertices);

    // Create vertex buffer
    size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    vertexBuffer_ = GpuBuffer({
        .device = &device,
        .size = vertexBufferSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .allocFlags = 0,
        .memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        .persistentMap = false,
        .debugName = debugName + "_vb"
    });

    // Upload vertex data
    auto vertexStagingLoc = uploader.write(vertices.data(), vertexBufferSize);
    uploader.copyBuffer(vertexBuffer_.handle(), 0, vertexStagingLoc);

    // Create index buffer (if indices provided)
    if (indexCount_ > 0) {
        size_t indexBufferSize = indices.size() * sizeof(uint32_t);
        indexBuffer_ = GpuBuffer({
            .device = &device,
            .size = indexBufferSize,
            .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            .allocFlags = 0,
            .memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            .persistentMap = false,
            .debugName = debugName + "_ib"
        });

        // Upload index data
        auto indexStagingLoc = uploader.write(indices.data(), indexBufferSize);
        uploader.copyBuffer(indexBuffer_.handle(), 0, indexStagingLoc);
    }
}

void Mesh::draw(CmdList& cmd, bool bindMaterial, VkPipelineLayout materialSetLayout) const {
    // Bind vertex buffer
    cmd.bindVertexBuffer(0, vertexBuffer_.handle(), 0);

    // Bind material descriptor set if requested
    if (bindMaterial && material_ && material_->descriptorSet() != VK_NULL_HANDLE) {
        VkDescriptorSet matSet = material_->descriptorSet();
        vkCmdBindDescriptorSets(cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                                materialSetLayout, 1, 1, &matSet, 0, nullptr);
    }

    // Draw indexed or non-indexed
    if (indexCount_ > 0) {
        cmd.bindIndexBuffer(indexBuffer_.handle(), 0, VK_INDEX_TYPE_UINT32);
        cmd.drawIndexed(indexCount_);
    } else {
        cmd.draw(vertexCount_);
    }
}

void Mesh::computeBounds(const std::vector<Vertex>& vertices) {
    bounds_ = AABB();
    for (const auto& v : vertices) {
        bounds_.expand(v.position);
    }
}

Mesh MeshBuilder::build(const Device& device, StagingUploader& uploader) {
    Mesh mesh;
    mesh.create(device, uploader, vertices_, indices_, material_, name_);
    return mesh;
}

} // namespace hvk
