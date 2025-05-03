#ifndef HVK_FRAME_INFO
#define HVK_FRAME_INFO

#include "hvk_camera.h"
#include "hvk_game_object.h"

#include <vulkan/vulkan.h>

#define MAX_LIGHTS 100

namespace hvk {
	struct PointLight
	{
		glm::vec4 position{};  // ignore w
		glm::vec4 color{};     // w is intensity
	};

	struct GlobalUbo {
		glm::mat4 projection{ 1.f };
		glm::mat4 view{ 1.f };
		glm::mat4 inverseView{ 1.f };
		glm::vec4 ambientLightColor{ 1.f, 1.f, 1.f, .02f };  // w is intensity
		PointLight pointLights[MAX_LIGHTS];
		int numLights;
	};

	struct FrameInfo {
		int frameIndex;
		float frameTime;
		VkCommandBuffer commandBuffer;
		HvkCamera& camera;
		VkDescriptorSet globalDescriptorSet;
		HvkGameObject::Map& gameObjects;
	};
}

#endif // HVK_FRAME_INFO
