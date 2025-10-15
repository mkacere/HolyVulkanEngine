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

void Model::draw(
    CmdList& cmd,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet globalDescSet,
    const glm::mat4& modelTransform
) const {
    // Bind global descriptor set (Set 0: Scene + Camera + Lights)
    vkCmdBindDescriptorSets(cmd.handle(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            pipelineLayout, 0, 1, &globalDescSet, 0, nullptr);

    // Draw from root node
    if (rootNodeIndex_ >= 0 && rootNodeIndex_ < static_cast<int32_t>(nodes_.size())) {
        drawNode(rootNodeIndex_, cmd, pipelineLayout, globalDescSet, modelTransform);
    } else {
        // No root, draw all top-level nodes
        for (size_t i = 0; i < nodes_.size(); ++i) {
            if (nodes_[i].parentIndex == -1) {
                drawNode(static_cast<int32_t>(i), cmd, pipelineLayout, globalDescSet, modelTransform);
            }
        }
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

    // Draw mesh if this node has one
    if (node.meshIndex >= 0 && node.meshIndex < static_cast<int32_t>(meshes_.size())) {
        const Mesh& mesh = meshes_[node.meshIndex];

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
