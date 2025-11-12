#include <hvk/resources/hvk_model.h>
#include <hvk/gfx/hvk_utils.hpp>
#include <stdexcept>

namespace hvk {

void Model::updateTransforms() {
    if (rootNodeIndex_ >= 0 && rootNodeIndex_ < static_cast<int32_t>(nodes_.size())) {
        updateTransforms(rootNodeIndex_, glm::mat4(1.0f));
    } else {
        // No root, update all top-level nodes
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].parentIndex == -1) {
                updateTransforms(static_cast<int32_t>(i), glm::mat4(1.0f));
            }
        }
    }
}

void Model::updateTransforms(int32_t nodeIndex, const glm::mat4& parentTransform) {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(nodes_.size())) {
        return;
    }

    Node& node = nodes_[nodeIndex];
    node.worldTransform = parentTransform * node.localTransform;

    // Recursively update children
    for (int32_t childIndex : node.children) {
        updateTransforms(childIndex, node.worldTransform);
    }
}

AABB Model::worldBounds() const {
    AABB worldAABB;  // Starts with min=FLT_MAX, max=-FLT_MAX

    // Helper to transform AABB by a matrix
    // To properly transform an AABB, we need to transform all 8 corners
    auto transformAABB = [](const AABB& localBounds, const glm::mat4& transform) -> AABB {
        if (!localBounds.isValid()) {
            return AABB();  // Return empty if input is invalid
        }

        AABB result;

        // Transform all 8 corners of the AABB
        glm::vec3 corners[8] = {
            glm::vec3(localBounds.min.x, localBounds.min.y, localBounds.min.z),
            glm::vec3(localBounds.max.x, localBounds.min.y, localBounds.min.z),
            glm::vec3(localBounds.min.x, localBounds.max.y, localBounds.min.z),
            glm::vec3(localBounds.max.x, localBounds.max.y, localBounds.min.z),
            glm::vec3(localBounds.min.x, localBounds.min.y, localBounds.max.z),
            glm::vec3(localBounds.max.x, localBounds.min.y, localBounds.max.z),
            glm::vec3(localBounds.min.x, localBounds.max.y, localBounds.max.z),
            glm::vec3(localBounds.max.x, localBounds.max.y, localBounds.max.z)
        };

        for (const auto& corner : corners) {
            glm::vec4 transformed = transform * glm::vec4(corner, 1.0f);
            result.expand(glm::vec3(transformed) / transformed.w);
        }

        return result;
    };

    // Traverse all nodes and collect transformed mesh bounds
    for (const Node& node : nodes_) {
        if (node.meshIndex >= 0 && node.meshIndex < static_cast<int32_t>(meshes_.size())) {
            const Mesh& mesh = meshes_[node.meshIndex];
            AABB transformedBounds = transformAABB(mesh.bounds(), node.worldTransform);
            worldAABB.expand(transformedBounds);
        }
    }

    return worldAABB;
}

void Model::draw(
    CmdList& cmd,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet globalDescSet,
    const glm::mat4& modelTransform
) const {
    // Bind global descriptor set (Set 0: Scene + Camera + Lights)
    vkCmdBindDescriptorSets(cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &globalDescSet, 0, nullptr);

    // DEBUG: Log draw start
    static bool firstFrame = true;
    if (firstFrame) {
        std::cout << "\n=== Model::draw() DEBUG ===" << std::endl;
        std::cout << "  Total nodes: " << nodes_.size() << std::endl;
        std::cout << "  Total meshes: " << meshes_.size() << std::endl;
        std::cout << "  Root node index: " << rootNodeIndex_ << std::endl;
    }

    // Draw from root node (if set) AND any orphaned nodes
    // IMPORTANT: Some GLTF files have both scene root nodes AND orphaned nodes.
    // We must draw ALL top-level nodes (parentIndex == -1) to ensure everything renders.
    if (rootNodeIndex_ >= 0 && rootNodeIndex_ < static_cast<int32_t>(nodes_.size())) {
        if (firstFrame) std::cout << "  Drawing from root node " << rootNodeIndex_ << std::endl;
        drawNode(rootNodeIndex_, cmd, pipelineLayout, globalDescSet, modelTransform);

        // Also draw any OTHER top-level nodes (orphaned nodes not in the main hierarchy)
        if (firstFrame) std::cout << "  Drawing additional top-level nodes (if any):" << std::endl;
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].parentIndex == -1 && static_cast<int32_t>(i) != rootNodeIndex_) {
                if (firstFrame) std::cout << "    Orphaned node " << i << ": " << nodes_[i].name << std::endl;
                drawNode(static_cast<int32_t>(i), cmd, pipelineLayout, globalDescSet, modelTransform);
            }
        }
    } else {
        // No root, draw all top-level nodes
        if (firstFrame) std::cout << "  No root, drawing all top-level nodes:" << std::endl;
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].parentIndex == -1) {
                if (firstFrame) std::cout << "    Top-level node " << i << ": " << nodes_[i].name << std::endl;
                drawNode(static_cast<int32_t>(i), cmd, pipelineLayout, globalDescSet, modelTransform);
            }
        }
    }

    if (firstFrame) {
        std::cout << "=== End Model::draw() DEBUG ===" << std::endl;
        firstFrame = false;
    }
}

void Model::drawNode(
    int32_t nodeIndex,
    CmdList& cmd,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet globalDescSet,
    const glm::mat4& modelTransform
) const {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(nodes_.size())) {
        return;
    }

    const Node& node = nodes_[nodeIndex];
    // Combine the model transform with the precomputed world transform once.
    // Do not reapply parent transforms recursively (worldTransform already includes hierarchy).
    glm::mat4 nodeTransform = modelTransform * node.worldTransform;

    // DEBUG: Log node draw (first frame only)
    static bool firstFrame = true;

    // Draw mesh if this node has one
    if (node.meshIndex >= 0 && node.meshIndex < static_cast<int32_t>(meshes_.size())) {
        const Mesh& mesh = meshes_[node.meshIndex];

        if (firstFrame) {
            const char* materialName = mesh.material() ? mesh.material()->name().c_str() : "none";
            std::cout << "  Drawing node '" << node.name << "' (idx " << nodeIndex
                      << ") -> mesh " << node.meshIndex << " with material '" << materialName << "'" << std::endl;
        }

        // Compute normal matrix (inverse transpose of upper-left 3x3)
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(nodeTransform)));
        glm::mat4 normalMatrix = glm::mat4(normalMat);

        // Get material parameters (use default if no material)
        MaterialParams materialParams;
        if (mesh.material()) {
            materialParams = mesh.material()->params();
        }

        // Push constants: model matrix + normal matrix + material params (208 bytes)
        struct PushConstants {
            glm::mat4 model;            // 64 bytes
            glm::mat4 normalMatrix;     // 64 bytes
            MaterialParams material;    // 80 bytes
        } pc;
        pc.model = nodeTransform;
        pc.normalMatrix = normalMatrix;
        pc.material = materialParams;

        vkCmdPushConstants(cmd.handle(), pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pc);

        // Draw mesh (will bind material descriptor set if needed)
        mesh.draw(cmd, true, pipelineLayout);
    }

    // Recursively draw children
    for (int32_t childIndex : node.children) {
        drawNode(childIndex, cmd, pipelineLayout, globalDescSet, modelTransform);
    }

    // DEBUG: Mark first frame complete (do this once for the whole tree)
    if (firstFrame && nodeIndex == 0) {
        firstFrame = false;
    }
}

void Model::drawOpaque(
    CmdList& cmd,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet globalDescSet,
    const glm::mat4& modelTransform
) const {
    // OPTIMIZATION: Global descriptor set is already bound by MeshRenderSystem
    // No need to rebind it here (saves ~N driver calls per frame where N = entity count)

    // Draw from root node or all top-level nodes
    if (rootNodeIndex_ >= 0 && rootNodeIndex_ < static_cast<int32_t>(nodes_.size())) {
        drawNodeFiltered(rootNodeIndex_, cmd, pipelineLayout, globalDescSet, modelTransform, AlphaMode::Opaque);

        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].parentIndex == -1 && static_cast<int32_t>(i) != rootNodeIndex_) {
                drawNodeFiltered(static_cast<int32_t>(i), cmd, pipelineLayout, globalDescSet, modelTransform, AlphaMode::Opaque);
            }
        }
    } else {
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].parentIndex == -1) {
                drawNodeFiltered(static_cast<int32_t>(i), cmd, pipelineLayout, globalDescSet, modelTransform, AlphaMode::Opaque);
            }
        }
    }
}

void Model::drawMasked(
    CmdList& cmd,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet globalDescSet,
    const glm::mat4& modelTransform
) const {
    // OPTIMIZATION: Global descriptor set is already bound by MeshRenderSystem
    // No need to rebind it here (saves ~N driver calls per frame where N = entity count)

    // Draw from root node or all top-level nodes
    if (rootNodeIndex_ >= 0 && rootNodeIndex_ < static_cast<int32_t>(nodes_.size())) {
        drawNodeFiltered(rootNodeIndex_, cmd, pipelineLayout, globalDescSet, modelTransform, AlphaMode::Mask);

        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].parentIndex == -1 && static_cast<int32_t>(i) != rootNodeIndex_) {
                drawNodeFiltered(static_cast<int32_t>(i), cmd, pipelineLayout, globalDescSet, modelTransform, AlphaMode::Mask);
            }
        }
    } else {
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].parentIndex == -1) {
                drawNodeFiltered(static_cast<int32_t>(i), cmd, pipelineLayout, globalDescSet, modelTransform, AlphaMode::Mask);
            }
        }
    }
}

void Model::drawBlended(
    CmdList& cmd,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet /*globalDescSet*/,  // Already bound by MeshRenderSystem
    const glm::vec3& cameraPosition,
    const glm::mat4& modelTransform
) const {
    // OPTIMIZATION: Global descriptor set is already bound by MeshRenderSystem
    // No need to rebind it here (saves ~N driver calls per frame where N = entity count)

    // === TRANSPARENCY SORTING ===
    // Collect all blended meshes with their distances from camera
    struct TransparentMesh {
        int32_t nodeIndex;
        glm::vec3 worldPosition;
        float distanceFromCamera;
    };

    std::vector<TransparentMesh> transparentMeshes;
    transparentMeshes.reserve(32); // Reasonable default

    // Helper lambda to collect transparent meshes
    auto collectTransparentMeshes = [&](auto& self, int32_t nodeIndex) -> void {
        if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(nodes_.size())) {
            return;
        }

        const Node& node = nodes_[nodeIndex];
        glm::mat4 nodeTransform = modelTransform * node.worldTransform;

        // Check if this node has a blended mesh
        if (node.meshIndex >= 0 && node.meshIndex < static_cast<int32_t>(meshes_.size())) {
            const Mesh& mesh = meshes_[node.meshIndex];

            bool isBlended = false;
            if (mesh.material()) {
                isBlended = (mesh.material()->alphaMode() == AlphaMode::Blend);
            }

            if (isBlended) {
                // Calculate world position (center of mesh bounds)
                glm::vec3 localCenter = (mesh.bounds().min + mesh.bounds().max) * 0.5f;
                glm::vec4 worldPos4 = nodeTransform * glm::vec4(localCenter, 1.0f);
                glm::vec3 worldPos = glm::vec3(worldPos4) / worldPos4.w;

                // Calculate distance from camera
                float distance = glm::length(worldPos - cameraPosition);

                transparentMeshes.push_back({nodeIndex, worldPos, distance});
            }
        }

        // Recursively collect from children
        for (int32_t childIndex : node.children) {
            self(self, childIndex);
        }
    };

    // Collect all transparent meshes from scene graph
    if (rootNodeIndex_ >= 0 && rootNodeIndex_ < static_cast<int32_t>(nodes_.size())) {
        collectTransparentMeshes(collectTransparentMeshes, rootNodeIndex_);

        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].parentIndex == -1 && static_cast<int32_t>(i) != rootNodeIndex_) {
                collectTransparentMeshes(collectTransparentMeshes, static_cast<int32_t>(i));
            }
        }
    } else {
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].parentIndex == -1) {
                collectTransparentMeshes(collectTransparentMeshes, static_cast<int32_t>(i));
            }
        }
    }

    // Sort back-to-front (farthest first)
    std::sort(transparentMeshes.begin(), transparentMeshes.end(),
        [](const TransparentMesh& a, const TransparentMesh& b) {
            return a.distanceFromCamera > b.distanceFromCamera; // Descending order
        });

    // Draw sorted transparent meshes
    for (const auto& tm : transparentMeshes) {
        const Node& node = nodes_[tm.nodeIndex];
        glm::mat4 nodeTransform = modelTransform * node.worldTransform;

        const Mesh& mesh = meshes_[node.meshIndex];

        // Compute normal matrix
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(nodeTransform)));
        glm::mat4 normalMatrix = glm::mat4(normalMat);

        // Get material parameters
        MaterialParams materialParams;
        if (mesh.material()) {
            materialParams = mesh.material()->params();
        }

        // Push constants
        struct PushConstants {
            glm::mat4 model;
            glm::mat4 normalMatrix;
            MaterialParams material;
        } pc;
        pc.model = nodeTransform;
        pc.normalMatrix = normalMatrix;
        pc.material = materialParams;

        vkCmdPushConstants(cmd.handle(), pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pc);

        mesh.draw(cmd, true, pipelineLayout);
    }
}

void Model::drawNodeFiltered(
    int32_t nodeIndex,
    CmdList& cmd,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet globalDescSet,
    const glm::mat4& modelTransform,
    AlphaMode filterMode
) const {
    if (nodeIndex < 0 || nodeIndex >= static_cast<int32_t>(nodes_.size())) {
        return;
    }

    const Node& node = nodes_[nodeIndex];
    glm::mat4 nodeTransform = modelTransform * node.worldTransform;

    // Draw mesh if this node has one AND it matches the filter mode
    if (node.meshIndex >= 0 && node.meshIndex < static_cast<int32_t>(meshes_.size())) {
        const Mesh& mesh = meshes_[node.meshIndex];

        // Check if this mesh's material matches the filter
        bool shouldDraw = false;
        if (mesh.material()) {
            shouldDraw = (mesh.material()->alphaMode() == filterMode);
        } else {
            // No material = treat as opaque
            shouldDraw = (filterMode == AlphaMode::Opaque);
        }

        if (shouldDraw) {
            // Compute normal matrix
            glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(nodeTransform)));
            glm::mat4 normalMatrix = glm::mat4(normalMat);

            // Get material parameters
            MaterialParams materialParams;
            if (mesh.material()) {
                materialParams = mesh.material()->params();
            }

            // Push constants
            struct PushConstants {
                glm::mat4 model;
                glm::mat4 normalMatrix;
                MaterialParams material;
            } pc;
            pc.model = nodeTransform;
            pc.normalMatrix = normalMatrix;
            pc.material = materialParams;

            vkCmdPushConstants(cmd.handle(), pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &pc);

            mesh.draw(cmd, true, pipelineLayout);
        }
    }

    // Recursively draw children with same filter
    for (int32_t childIndex : node.children) {
        drawNodeFiltered(childIndex, cmd, pipelineLayout, globalDescSet, modelTransform, filterMode);
    }
}

void Model::createDefaultTextures(
    const Device& device,
    StagingUploader& uploader,
    SamplerCache& samplerCache
) {
    // White texture (1x1, RGBA = 255,255,255,255)
    // Note: This is used as default albedo when no texture is present
    {
        uint8_t whitePixel[4] = {255, 255, 255, 255};
        uploader.beginFrame(0);
        defaultWhite_ = Texture::createFromMemory(
            device, uploader, samplerCache,
            whitePixel, {1, 1}, VK_FORMAT_R8G8B8A8_SRGB,  // Changed to SRGB
            false, "default_white"
        );
        uploader.submit();
        uploader.waitCurrent();
    }

    // Normal texture (1x1, RGB = 128,128,255 -> (0, 0, 1) in tangent space)
    {
        uint8_t normalPixel[4] = {128, 128, 255, 255};
        uploader.beginFrame(0);
        defaultNormal_ = Texture::createFromMemory(
            device, uploader, samplerCache,
            normalPixel, {1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
            false, "default_normal"
        );
        uploader.submit();
        uploader.waitCurrent();
    }

    // Metallic-Roughness texture (1x1, B=0 (non-metallic), G=128 (medium roughness), R=255, A=255)
    // Note: GLTF uses B channel for metallic, G channel for roughness
    {
        uint8_t mrPixel[4] = {255, 128, 0, 255};  // Changed G from 255 to 128 (0.5 roughness)
        uploader.beginFrame(0);
        defaultMetallicRoughness_ = Texture::createFromMemory(
            device, uploader, samplerCache,
            mrPixel, {1, 1}, VK_FORMAT_R8G8B8A8_UNORM,
            false, "default_mr"
        );
        uploader.submit();
        uploader.waitCurrent();
    }
}

} // namespace hvk
