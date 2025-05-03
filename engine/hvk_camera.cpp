#include "hvk_camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>

namespace hvk {
	void HvkCamera::setOrthographicProjection(float left, float right, float top, float bottom, float nearPlane, float farPlane) {
		projectionMatrix_ = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
	}
	void HvkCamera::setPerspectiveProjection(float fovY, float aspect, float nearPlane, float farPlane) {
		projectionMatrix_ = glm::perspective(fovY, aspect, nearPlane, farPlane);
	}
	void HvkCamera::setViewDirection(const glm::vec3& position, const glm::vec3& direction, const glm::vec3& up) {
		viewMatrix_ = glm::lookAt(position, position + direction, up);
		inverseViewMatrix_ = glm::inverse(viewMatrix_);
	}
	void HvkCamera::setViewTarget(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) {
		viewMatrix_ = glm::lookAt(position, target, up);
		inverseViewMatrix_ = glm::inverse(viewMatrix_);
	}
	void HvkCamera::setViewXYZ(const glm::vec3& position, const glm::vec3& rotation) {
		glm::mat4 T = glm::translate(glm::mat4(1.f), position);
		glm::mat4 Rx = glm::rotate(glm::mat4(1.f), rotation.x, glm::vec3(1, 0, 0));
		glm::mat4 Ry = glm::rotate(glm::mat4(1.f), rotation.y, glm::vec3(0, 1, 0));
		glm::mat4 Rz = glm::rotate(glm::mat4(1.f), rotation.z, glm::vec3(0, 0, 1));
		inverseViewMatrix_ = T * Rz * Ry * Rx;
		viewMatrix_ = glm::inverse(inverseViewMatrix_);
	}

}
