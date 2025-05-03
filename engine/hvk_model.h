#ifndef HVK_MODEL
#define HVK_MODEL

#include "hvk_buffer.h"
#include "hvk_device.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace hvk {
	class HvkModel
	{
	public:

		struct Vertex {
			glm::vec3 position{};
			glm::vec3 color{};
			glm::vec3 normal{};
			glm::vec2 uv{};

			static std::vector<VkVertexInputBindingDescription> getBindingDescriptions() {
				VkVertexInputBindingDescription desc{};
				desc.binding = 0;
				desc.stride = sizeof(Vertex);
				desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
				return { desc };
			}

			static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions() {
				std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

				attributeDescriptions.push_back({ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position) });
				attributeDescriptions.push_back({ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color) });
				attributeDescriptions.push_back({ 2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal) });
				attributeDescriptions.push_back({ 3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv) });

				return attributeDescriptions;
			};

			bool operator==(const Vertex& other) const {
				return position == other.position && color == other.color && normal == other.normal &&
					uv == other.uv;
			}
		};

		struct Builder {
			std::vector<Vertex> vertices{};
			std::vector<uint32_t> indices{};

			void loadModel(const std::string& filepath);
		};


		HvkModel(HvkDevice& device, const HvkModel::Builder& builder);
		~HvkModel();

		HvkModel(const HvkModel&) = delete;
		HvkModel& operator=(const HvkModel&) = delete;

		static std::unique_ptr<HvkModel> createModelFromFile(HvkDevice& device, const std::string& filepath);

		void bind(VkCommandBuffer commandBuffer);
		void draw(VkCommandBuffer commandBuffer);

	private:
		void createVertexBuffers(const std::vector<Vertex>& vertices);
		void createIndexBuffers(const std::vector<uint32_t>& indices);

		HvkDevice& hvkDevice_;

		std::unique_ptr<HvkBuffer> vertexBuffer_;
		uint32_t vertexCount_;

		bool hasIndexBuffer_ = false;
		std::unique_ptr<HvkBuffer> indexBuffer_;
		uint32_t indexCount_;


	};
}

#endif // HVK_MODEL