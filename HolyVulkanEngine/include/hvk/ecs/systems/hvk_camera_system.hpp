/**
 * @file hvk_camera_system.hpp
 * @brief Camera update system
 * @author Holy Vulkan Engine
 * @date 2025
 * Updates camera data and manages active camera selection.
 */

#ifndef HVK_ECS_CAMERA_SYSTEM_HPP
#define HVK_ECS_CAMERA_SYSTEM_HPP

#include <hvk/ecs/hvk_world.hpp>
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace hvk {

/**
 * CameraSystem - Computes camera matrices from CameraComponent + TransformComponent
 *
 * Responsibilities:
 * - Compute view matrices from TransformComponent (camera position/orientation)
 * - Compute projection matrices from CameraComponent (FOV, aspect, near/far)
 * - Store results in RenderCamera component for rendering systems
 *
 * Component requirements:
 * - CameraComponent (input - projection settings)
 * - TransformComponent (input - camera position/rotation)
 * - RenderCamera (output - computed view/proj matrices)
 *
 * Performance:
 * - Only updates cameras that changed
 * - Uses EnTT view for cache-friendly iteration
 */
class CameraSystem : public ILogicSystem {
public:
    CameraSystem() = default;
    ~CameraSystem() override = default;

    void update(entt::registry& registry, float deltaTime) override;

private:
    // Helper: Compute view matrix from transform
    static glm::mat4 computeViewMatrix(const glm::vec3& position, const glm::quat& rotation);

    // Helper: Compute perspective projection (Vulkan-style: Y-down, Z:0-1)
    static glm::mat4 computePerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane);

    // Helper: Compute orthographic projection (Vulkan-style)
    static glm::mat4 computeOrthographic(float width, float aspect, float nearPlane, float farPlane);

    // Helper: Extract frustum planes from view-projection matrix
    static void extractFrustumPlanes(const glm::mat4& viewProj, glm::vec4 planes[6]);
};

} // namespace hvk

#endif // HVK_ECS_CAMERA_SYSTEM_HPP
