#include <hvk/ecs/systems/hvk_camera_system.hpp>
#include <hvk/ecs/hvk_components.hpp>
#include <hvk/ecs/hvk_render_components.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace hvk {

void CameraSystem::update(entt::registry& registry, float /*deltaTime*/) {
    

    // Iterate all cameras (CameraComponent + TransformComponent)
    auto view = registry.view<CameraComponent, TransformComponent>();

    for (auto entity : view) {
        const auto& camera = view.get<CameraComponent>(entity);
        const auto& transform = view.get<TransformComponent>(entity);

        // Compute view matrix from transform
        glm::mat4 viewMatrix = computeViewMatrix(transform.position, transform.rotation);

        // Compute projection matrix based on camera type
        glm::mat4 projMatrix;
        if (camera.type == CameraComponent::Type::Perspective) {
            projMatrix = computePerspective(
                camera.fovYDegrees,
                camera.aspectRatio,
                camera.nearPlane,
                camera.farPlane
            );
        } else {
            projMatrix = computeOrthographic(
                camera.orthoWidth,
                camera.aspectRatio,
                camera.nearPlane,
                camera.farPlane
            );
        }

        // Combine view-projection
        glm::mat4 viewProj = projMatrix * viewMatrix;

        // Create/update RenderCamera component
        RenderCamera renderCam;
        renderCam.view = viewMatrix;
        renderCam.projection = projMatrix;
        renderCam.viewProjection = viewProj;

        // Extract frustum planes for culling
        extractFrustumPlanes(viewProj, renderCam.frustumPlanes);

        // Store in RenderCamera component
        registry.emplace_or_replace<RenderCamera>(entity, renderCam);
    }
}

// ============================================================================
// Helper Functions
// ============================================================================

glm::mat4 CameraSystem::computeViewMatrix(const glm::vec3& position, const glm::quat& rotation) {
    // Camera view matrix: inverse of camera's world transform
    // World transform: T * R
    // View transform: R^-1 * T^-1

    // Compute rotation matrix from quaternion
    glm::mat4 rotationMatrix = glm::mat4_cast(rotation);

    // Compute translation
    glm::mat4 translationMatrix = glm::translate(glm::mat4(1.0f), position);

    // View matrix = inverse(translation * rotation)
    // = inverse(rotation) * inverse(translation)
    glm::mat4 worldToCamera = glm::inverse(translationMatrix * rotationMatrix);

    return worldToCamera;
}

glm::mat4 CameraSystem::computePerspective(float fovYDegrees, float aspect, float nearPlane, float farPlane) {
    // Vulkan-style perspective projection:
    // - Y-axis points down in NDC (negative viewport height flips it)
    // - Z-axis range: [0, 1] (not [-1, 1] like OpenGL)
    // - Right-handed coordinate system

    // GLM's perspectiveFov expects radians
    float fovYRadians = glm::radians(fovYDegrees);

    // GLM provides glm::perspectiveLH_ZO (left-handed, Z:0-1) and glm::perspectiveRH_ZO (right-handed, Z:0-1)
    // We want right-handed with Z:[0,1] for Vulkan
    glm::mat4 proj = glm::perspectiveRH_ZO(fovYRadians, aspect, nearPlane, farPlane);

    // Vulkan's Y-axis is flipped compared to OpenGL
    // Flip Y-axis in projection to match Vulkan's coordinate system
    proj[1][1] *= -1.0f;

    return proj;
}

glm::mat4 CameraSystem::computeOrthographic(float width, float aspect, float nearPlane, float farPlane) {
    // Orthographic projection for Vulkan
    float height = width / aspect;

    float left = -width * 0.5f;
    float right = width * 0.5f;
    float bottom = -height * 0.5f;
    float top = height * 0.5f;

    // GLM provides orthoRH_ZO (right-handed, Z:0-1) for Vulkan
    glm::mat4 proj = glm::orthoRH_ZO(left, right, bottom, top, nearPlane, farPlane);

    // Flip Y-axis for Vulkan
    proj[1][1] *= -1.0f;

    return proj;
}

void CameraSystem::extractFrustumPlanes(const glm::mat4& viewProj, glm::vec4 planes[6]) {
    // Extract frustum planes from view-projection matrix
    // Plane equation: Ax + By + Cz + D = 0
    // Stored as vec4(A, B, C, D)

    // Left plane
    planes[0] = glm::vec4(
        viewProj[0][3] + viewProj[0][0],
        viewProj[1][3] + viewProj[1][0],
        viewProj[2][3] + viewProj[2][0],
        viewProj[3][3] + viewProj[3][0]
    );

    // Right plane
    planes[1] = glm::vec4(
        viewProj[0][3] - viewProj[0][0],
        viewProj[1][3] - viewProj[1][0],
        viewProj[2][3] - viewProj[2][0],
        viewProj[3][3] - viewProj[3][0]
    );

    // Bottom plane
    planes[2] = glm::vec4(
        viewProj[0][3] + viewProj[0][1],
        viewProj[1][3] + viewProj[1][1],
        viewProj[2][3] + viewProj[2][1],
        viewProj[3][3] + viewProj[3][1]
    );

    // Top plane
    planes[3] = glm::vec4(
        viewProj[0][3] - viewProj[0][1],
        viewProj[1][3] - viewProj[1][1],
        viewProj[2][3] - viewProj[2][1],
        viewProj[3][3] - viewProj[3][1]
    );

    // Near plane
    planes[4] = glm::vec4(
        viewProj[0][2],
        viewProj[1][2],
        viewProj[2][2],
        viewProj[3][2]
    );

    // Far plane
    planes[5] = glm::vec4(
        viewProj[0][3] - viewProj[0][2],
        viewProj[1][3] - viewProj[1][2],
        viewProj[2][3] - viewProj[2][2],
        viewProj[3][3] - viewProj[3][2]
    );

    // Normalize planes
    for (int i = 0; i < 6; i++) {
        float length = glm::length(glm::vec3(planes[i]));
        if (length > 0.0f) {
            planes[i] /= length;
        }
    }
}

} // namespace hvk
