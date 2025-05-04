#ifndef HVK_MODEL
#define HVK_MODEL

#include "hvk_buffer.h"
#include "hvk_device.h"
#include "hvk_descriptors.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <tiny_gltf.h>
#include <stdexcept>

namespace hvk {

    class HvkModel {
    public:
        struct Vertex {
            glm::vec3 position{};
            glm::vec3 color{};
            glm::vec3 normal{};
            glm::vec2 uv{};

            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions() {
                return { {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX} };
            }

            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
                return {
                    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
                    {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)},
                    {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
                    {3, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, uv)},
                };
            }

            bool operator==(Vertex const& other) const {
                return position == other.position && color == other.color
                    && normal == other.normal && uv == other.uv;
            }
        };

        struct Builder {
            tinygltf::Model model;
            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;
            // PBR slots
            bool hasBaseColor = false;
            bool hasMR = false;
            bool hasNormalMap = false;
            bool hasEmissive = false;
            const tinygltf::Image* baseColorImage = nullptr;
            const tinygltf::Image* mrImage = nullptr;
            const tinygltf::Image* normalImage = nullptr;
            const tinygltf::Image* emissiveImage = nullptr;
            glm::vec4 baseColorFactor{ 1.f,1.f,1.f,1.f };
            glm::vec2 texCoordBase{ 0.f,0.f }, texCoordMR{ 0.f,0.f }, texCoordNM{ 0.f,0.f }, texCoordEM{ 0.f,0.f };

            void loadModel(std::string const& filepath);
        };

        HvkModel(HvkDevice& device, Builder const& builder);
        ~HvkModel();
        HvkModel(HvkModel const&) = delete;
        HvkModel& operator=(HvkModel const&) = delete;

        static std::unique_ptr<HvkModel> createModelFromFile(HvkDevice& dev, std::string const& path) {
            Builder builder;
            builder.loadModel(path);
            return std::make_unique<HvkModel>(dev, builder);
        }

        void bind(VkCommandBuffer cmd, VkPipelineLayout layout) const;
        void draw(VkCommandBuffer cmd) const;

        // descriptor helpers
        bool hasTexture() const { return !imageInfos_.empty(); }
        void writeDescriptors(VkDescriptorSet set) const;

        VkDescriptorImageInfo getImageInfo() const {
            if (imageInfos_.empty()) throw std::runtime_error("no texture");
            return imageInfos_[0];
        }

        // descriptor setup
        void setDescriptorLayout(std::shared_ptr<HvkDescriptorSetLayout> layout) { descriptorSetLayout_ = std::move(layout); }
        void setDescriptorPool(std::shared_ptr<HvkDescriptorPool> pool) { descriptorPool_ = std::move(pool); }

    private:
        void createVertexBuffers(std::vector<Vertex> const& verts);
        void createIndexBuffers(std::vector<uint32_t> const& inds);
        void createTextureResources(Builder const& b);

        HvkDevice& device_;
        std::unique_ptr<HvkBuffer> vertexBuffer_;
        std::unique_ptr<HvkBuffer> indexBuffer_;
        uint32_t vertexCount_ = 0;
        uint32_t indexCount_ = 0;

        // each descriptor binding slot -> image
        std::vector<VkImage>        images_;
        std::vector<VkDeviceMemory> imageMemories_;
        std::vector<VkImageView>    imageViews_;
        std::vector<VkSampler>      samplers_;
        std::vector<VkDescriptorImageInfo> imageInfos_;

        std::shared_ptr<HvkDescriptorSetLayout> descriptorSetLayout_;
        std::shared_ptr<HvkDescriptorPool> descriptorPool_;
    };

} // namespace hvk
#endif // HVK_MODEL