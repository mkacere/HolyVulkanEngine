#ifndef HVK_CAMERA_HPP
#define HVK_CAMERA_HPP

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <hvk/gfx/hvk_camera_data.hpp>

namespace hvk {

/**
 * ProjectionType - Type of camera projection
 */
enum class ProjectionType {
    Perspective,
    Orthographic
};

/**
 * Camera - Pure data class for camera state (ECS-ready)
 *
 * Design principles:
 * - Separate data from behavior (no GLFW, no input handling)
 * - Can be used as ECS component directly
 * - CameraController (separate class) will manipulate Camera data
 * - Utility functions for computing matrices and populating CameraData UBO
 */
class Camera {
public:
    // --- Construction ---
    Camera() = default;

    /**
     * Create perspective camera
     *
     * @param position Camera position in world space
     * @param target Look-at target point
     * @param fovYDegrees Vertical field of view in degrees
     * @param aspectRatio Width/height ratio
     * @param nearPlane Near clipping plane
     * @param farPlane Far clipping plane
     */
    static Camera createPerspective(
        const glm::vec3& position,
        const glm::vec3& target,
        float fovYDegrees,
        float aspectRatio,
        float nearPlane = 0.1f,
        float farPlane = 1000.0f
    );

    /**
     * Create orthographic camera
     *
     * @param position Camera position in world space
     * @param target Look-at target point
     * @param orthoWidth Width of orthographic view volume
     * @param aspectRatio Width/height ratio
     * @param nearPlane Near clipping plane
     * @param farPlane Far clipping plane
     */
    static Camera createOrthographic(
        const glm::vec3& position,
        const glm::vec3& target,
        float orthoWidth,
        float aspectRatio,
        float nearPlane = 0.1f,
        float farPlane = 1000.0f
    );

    // --- Position and Orientation ---

    void setPosition(const glm::vec3& pos) { position_ = pos; updateViewMatrix(); }
    const glm::vec3& position() const { return position_; }

    void setRotation(const glm::quat& rot) { rotation_ = rot; updateViewMatrix(); }
    const glm::quat& rotation() const { return rotation_; }

    /**
     * Set camera orientation by specifying look-at target and up direction
     *
     * @param target World space point to look at
     * @param up Up direction (default: +Y)
     */
    void lookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

    /**
     * Set camera rotation using Euler angles (in degrees)
     *
     * @param pitch Rotation around right axis (X)
     * @param yaw Rotation around up axis (Y)
     * @param roll Rotation around forward axis (Z)
     */
    void setEulerAngles(float pitch, float yaw, float roll);

    /**
     * Get Euler angles from current rotation (in degrees)
     *
     * @return vec3(pitch, yaw, roll)
     */
    glm::vec3 eulerAngles() const;

    // --- Direction Vectors ---

    glm::vec3 forward() const { return rotation_ * glm::vec3(0.0f, 0.0f, -1.0f); }
    glm::vec3 right() const { return rotation_ * glm::vec3(1.0f, 0.0f, 0.0f); }
    glm::vec3 up() const { return rotation_ * glm::vec3(0.0f, 1.0f, 0.0f); }

    // --- Projection Parameters ---

    void setProjectionType(ProjectionType type) { projectionType_ = type; updateProjectionMatrix(); }
    ProjectionType projectionType() const { return projectionType_; }

    void setFovY(float fovYDegrees) { fovY_ = fovYDegrees; updateProjectionMatrix(); }
    float fovY() const { return fovY_; }

    void setAspectRatio(float aspect) { aspectRatio_ = aspect; updateProjectionMatrix(); }
    float aspectRatio() const { return aspectRatio_; }

    void setNearPlane(float nearPlane) { nearPlane_ = nearPlane; updateProjectionMatrix(); }
    float nearPlane() const { return nearPlane_; }

    void setFarPlane(float farPlane) { farPlane_ = farPlane; updateProjectionMatrix(); }
    float farPlane() const { return farPlane_; }

    void setOrthoWidth(float width) { orthoWidth_ = width; updateProjectionMatrix(); }
    float orthoWidth() const { return orthoWidth_; }

    /**
     * Update aspect ratio (typically called on window resize)
     *
     * @param width Viewport width in pixels
     * @param height Viewport height in pixels
     */
    void updateAspectRatio(uint32_t width, uint32_t height) {
        aspectRatio_ = static_cast<float>(width) / static_cast<float>(height);
        updateProjectionMatrix();
    }

    // --- Matrices ---

    const glm::mat4& viewMatrix() const { return viewMatrix_; }
    const glm::mat4& projectionMatrix() const { return projectionMatrix_; }
    glm::mat4 viewProjectionMatrix() const { return projectionMatrix_ * viewMatrix_; }

    // --- CameraData Population ---

    /**
     * Populate CameraData struct for uploading to GPU
     *
     * @param screenWidth Screen width in pixels
     * @param screenHeight Screen height in pixels
     * @return CameraData ready to upload to UBO
     */
    CameraData toCameraData(uint32_t screenWidth, uint32_t screenHeight) const;

private:
    // Position and orientation
    glm::vec3 position_{0.0f, 0.0f, 0.0f};
    glm::quat rotation_{1.0f, 0.0f, 0.0f, 0.0f};  // Identity quaternion

    // Projection parameters
    ProjectionType projectionType_ = ProjectionType::Perspective;
    float fovY_ = 60.0f;              // Vertical FOV in degrees (perspective)
    float aspectRatio_ = 16.0f / 9.0f;
    float nearPlane_ = 0.1f;
    float farPlane_ = 1000.0f;
    float orthoWidth_ = 10.0f;        // Width of orthographic view (orthographic)

    // Cached matrices
    glm::mat4 viewMatrix_{1.0f};
    glm::mat4 projectionMatrix_{1.0f};

    // Internal update methods
    void updateViewMatrix();
    void updateProjectionMatrix();
};

} // namespace hvk

#endif // HVK_CAMERA_HPP
