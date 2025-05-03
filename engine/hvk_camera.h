#ifndef HVK_CAMERA
#define HVK_CAMERA

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace hvk {
	class HvkCamera {
	public:
		void setOrthographicProjection(float left, float right, float top, float bottom, float nearPlane, float farPlane);
		void setPerspectiveProjection(float fovY, float aspect, float nearPlane, float farPlane);
		void setViewDirection(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& up = glm::vec3(0.f, -1.f, 0.f));
		void setViewTarget(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up = glm::vec3(0.f, -1.f, 0.f));
		void setViewXYZ(const glm::vec3& position, const glm::vec3& rotation);
		const glm::mat4& getProjection() const { return projectionMatrix_; }
		const glm::mat4& getView() const { return viewMatrix_; }
		const glm::mat4& getInverseView() const { return inverseViewMatrix_; }
		glm::vec3 getPosition() const { return glm::vec3(inverseViewMatrix_[3]); }
	private:
		glm::mat4 projectionMatrix_{ 1.f };
		glm::mat4 viewMatrix_{ 1.f };
		glm::mat4 inverseViewMatrix_{ 1.f };
	};
}

#endif // HVK_CAMERA