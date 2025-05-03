#include "hvk_model.h"

#include "hvk_utils.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cassert>
#include <cstring>
#include <unordered_map>

#ifndef ENGINE_DIR
#define ENGINE_DIR "../"
#endif
#include <iostream>

namespace std {
    template<>
    struct hash<hvk::HvkModel::Vertex> {
        size_t operator()(hvk::HvkModel::Vertex const& vertex) const {
            size_t seed = 0;
            hvk::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
            return seed;
        }
    };
}

namespace hvk {

    HvkModel::HvkModel(HvkDevice& device, const HvkModel::Builder& builder) :
        hvkDevice_(device)
    {
        createVertexBuffers(builder.vertices);
        createIndexBuffers(builder.indices);
    }

    HvkModel::~HvkModel()
    {
    }

    std::unique_ptr<HvkModel> HvkModel::createModelFromFile(HvkDevice& device, const std::string& filepath)
    {
        Builder builder{};
        builder.loadModel(ENGINE_DIR + filepath);
        return std::make_unique<HvkModel>(device, builder);
    }

    void HvkModel::bind(VkCommandBuffer commandBuffer)
    {
        VkBuffer buffers[] = { vertexBuffer_->getBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);

        if (hasIndexBuffer_) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer_->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    void HvkModel::draw(VkCommandBuffer commandBuffer)
    {
        if (hasIndexBuffer_) {
            vkCmdDrawIndexed(commandBuffer, indexCount_, 1, 0, 0, 0);
        }
        else {
            vkCmdDraw(commandBuffer, vertexCount_, 1, 0, 0);
        }
    }

    void HvkModel::createVertexBuffers(const std::vector<Vertex>& vertices)
    {
        vertexCount_ = static_cast<uint32_t>(vertices.size());
        assert(vertexCount_ >= 3 && "Vertex count must be at least 3");
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount_;
        uint32_t vertexSize = sizeof(vertices[0]);

        HvkBuffer stagingBuffer{
            hvkDevice_,
            vertexSize,
            vertexCount_,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*)vertices.data());

        vertexBuffer_ = std::make_unique<HvkBuffer>(
            hvkDevice_,
            vertexSize,
            vertexCount_,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        hvkDevice_.copyBuffer(stagingBuffer.getBuffer(), vertexBuffer_->getBuffer(), bufferSize);
    }

    void HvkModel::createIndexBuffers(const std::vector<uint32_t>& indices)
    {
        indexCount_ = static_cast<uint32_t>(indices.size());
        hasIndexBuffer_ = indexCount_ > 0;

        if (!hasIndexBuffer_) {
            return;
        }

        VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount_;
        uint32_t indexSize = sizeof(indices[0]);

        HvkBuffer stagingBuffer{
            hvkDevice_,
            indexSize,
            indexCount_,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*)indices.data());

        indexBuffer_ = std::make_unique<HvkBuffer>(
            hvkDevice_,
            indexSize,
            indexCount_,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        hvkDevice_.copyBuffer(stagingBuffer.getBuffer(), indexBuffer_->getBuffer(), bufferSize);
    }

    // chat gpt dont judge me,,, TODO read at some time lol
    void HvkModel::Builder::loadModel(const std::string& filepath)
    {
        tinygltf::Model model;
        tinygltf::TinyGLTF loader;
        std::string err, warn;
        bool ret;

        // Load binary or ASCII glTF
        {
            std::string ext = filepath.substr(filepath.find_last_of('.') + 1);
            if (ext == "glb") {
                ret = loader.LoadBinaryFromFile(&model, &err, &warn, filepath);
            }
            else {
                ret = loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
            }
        }

        if (!warn.empty()) std::cerr << "tinygltf warning: " << warn << "\n";
        if (!err.empty())  throw std::runtime_error("tinygltf error: " + err);
        if (!ret)         throw std::runtime_error("Failed to load glTF: " + filepath);

        vertices.clear();
        indices.clear();
        std::unordered_map<Vertex, uint32_t> uniqueVertices;

        // Helper to get raw data pointer for any accessor
        auto getDataPtr = [&](const tinygltf::Accessor& acc) {
            const auto& view = model.bufferViews[acc.bufferView];
            const auto& buffer = model.buffers[view.buffer];
            return buffer.data.data() + view.byteOffset + acc.byteOffset;
            };

        for (const auto& mesh : model.meshes) {
            for (const auto& prim : mesh.primitives) {
                // POSITION is required
                const auto& posAcc = model.accessors.at(prim.attributes.at("POSITION"));
                const float* positions = reinterpret_cast<const float*>(getDataPtr(posAcc));
                size_t vertCount = posAcc.count;

                // Optional attributes
                const tinygltf::Accessor* normAcc = prim.attributes.count("NORMAL")
                    ? &model.accessors.at(prim.attributes.at("NORMAL")) : nullptr;
                const tinygltf::Accessor* uvAcc = prim.attributes.count("TEXCOORD_0")
                    ? &model.accessors.at(prim.attributes.at("TEXCOORD_0")) : nullptr;
                const tinygltf::Accessor* colAcc = prim.attributes.count("COLOR_0")
                    ? &model.accessors.at(prim.attributes.at("COLOR_0")) : nullptr;

                // Lambda to build a Vertex from a glTF index
                auto buildVertex = [&](uint32_t idx) {
                    Vertex vertex{};
                    // position
                    vertex.position = {
                        positions[3 * idx + 0],
                        positions[3 * idx + 1],
                        positions[3 * idx + 2]
                    };
                    // normal
                    if (normAcc) {
                        const float* n = reinterpret_cast<const float*>(getDataPtr(*normAcc));
                        vertex.normal = {
                            n[3 * idx + 0],
                            n[3 * idx + 1],
                            n[3 * idx + 2]
                        };
                    }
                    // uv
                    if (uvAcc) {
                        const float* u = reinterpret_cast<const float*>(getDataPtr(*uvAcc));
                        vertex.uv = {
                            u[2 * idx + 0],
                            u[2 * idx + 1]
                        };
                    }
                    // color
                    if (colAcc) {
                        const float* c = reinterpret_cast<const float*>(getDataPtr(*colAcc));
                        vertex.color = {
                            c[3 * idx + 0],
                            c[3 * idx + 1],
                            c[3 * idx + 2]
                        };
                    }
                    return vertex;
                    };

                // If the primitive has an index buffer, iterate indices
                if (prim.indices > -1) {
                    const auto& idxAcc = model.accessors.at(prim.indices);
                    const void* idxData = getDataPtr(idxAcc);
                    for (size_t i = 0; i < idxAcc.count; ++i) {
                        uint32_t gltfIndex;
                        switch (idxAcc.componentType) {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
                            gltfIndex = reinterpret_cast<const uint16_t*>(idxData)[i];
                            break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
                            gltfIndex = reinterpret_cast<const uint32_t*>(idxData)[i];
                            break;
                        default:
                            throw std::runtime_error("Unsupported index type in glTF");
                        }

                        // Build, dedupe, and append
                        Vertex v = buildVertex(gltfIndex);
                        auto it = uniqueVertices.find(v);
                        if (it == uniqueVertices.end()) {
                            uint32_t newIndex = static_cast<uint32_t>(vertices.size());
                            uniqueVertices[v] = newIndex;
                            vertices.push_back(v);
                            indices.push_back(newIndex);
                        }
                        else {
                            indices.push_back(it->second);
                        }
                    }
                }
                // If no index data, just iterate every vertex
                else {
                    for (uint32_t idx = 0; idx < vertCount; ++idx) {
                        Vertex v = buildVertex(idx);
                        auto it = uniqueVertices.find(v);
                        if (it == uniqueVertices.end()) {
                            uint32_t newIndex = static_cast<uint32_t>(vertices.size());
                            uniqueVertices[v] = newIndex;
                            vertices.push_back(v);
                            indices.push_back(newIndex);
                        }
                        else {
                            indices.push_back(it->second);
                        }
                    }
                }
            }
        }
    }

}