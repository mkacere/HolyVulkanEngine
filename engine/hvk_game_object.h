#ifndef HVK_GAME_OBJECT
#define HVK_GAME_OBJECT

#include "hvk_model.h"

#include <glm/gtc/matrix_transform.hpp>

#include <memory>
#include <unordered_map>

namespace hvk {
	struct TransformComponent {
		glm::vec3 translation{};
		glm::vec3 scale{ 1.f, 1.f, 1.f };
		glm::vec3 rotation{};

		glm::mat4 mat4();

		glm::mat3 normalMatrix();
	};

	struct PointLightComponent
	{
		float lightIntensity = 1.0f;
	};

	class HvkGameObject
	{
	public:
		using id_t = unsigned int;
		using Map = std::unordered_map<id_t, HvkGameObject>;

		static HvkGameObject createGameObject() {
			static id_t currentId = 0;
			return HvkGameObject{ currentId++ };
		}

		static HvkGameObject makePointLight(float intensity = 10.f, float radius = 0.1f, glm::vec3 color = glm::vec3(1.f));

		HvkGameObject(const HvkGameObject&) = delete;
		HvkGameObject& operator=(const HvkGameObject&) = delete;
		HvkGameObject(HvkGameObject&&) = default;
		HvkGameObject& operator=(HvkGameObject&&) = default;
		
		id_t getId() const { return id_; }

		glm::vec3 color{};
		TransformComponent transform{};

		std::shared_ptr<HvkModel> model{};
		std::unique_ptr<PointLightComponent> pointLight = nullptr;

	private:
		HvkGameObject(id_t objId) : id_(objId) {}
		id_t id_;
	};
}


#endif // HVK_GAME_OBJECT
