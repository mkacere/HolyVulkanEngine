#include <hvk/scene/hvk_camera.hpp>

namespace hvk {

// --- Static Factory Methods ---

Camera Camera::createPerspective(
    const glm::vec3& position,
    const glm::vec3& target,
    float fovYDegrees,
    float aspectRatio,
    float nearPlane,
    float farPlane
) {
    Camera camera;
    camera.projectionType_ = ProjectionType::Perspective;
    camera.position_ = position;
    camera.fovY_ = fovYDegrees;
    camera.aspectRatio_ = aspectRatio;
    camera.nearPlane_ = nearPlane;
    camera.farPlane_ = farPlane;
    camera.lookAt(target);
    camera.updateProjectionMatrix();
    return camera;
}

Camera Camera::createOrthographic(
    const glm::vec3& position,
    const glm::vec3& target,
    float orthoWidth,
    float aspectRatio,
    float nearPlane,
    float farPlane
) {
    Camera camera;
    camera.projectionType_ = ProjectionType::Orthographic;
    camera.position_ = position;
    camera.orthoWidth_ = orthoWidth;
    camera.aspectRatio_ = aspectRatio;
    camera.nearPlane_ = nearPlane;
    camera.farPlane_ = farPlane;
    camera.lookAt(target);
    camera.updateProjectionMatrix();
    return camera;
}

// --- Orientation ---

void Camera::lookAt(const glm::vec3& target, const glm::vec3& up) {
    // Compute forward direction
    glm::vec3 forward = glm::normalize(target - position_);

    // Compute right and up vectors
    glm::vec3 right = glm::normalize(glm::cross(forward, up));
    glm::vec3 actualUp = glm::cross(right, forward);

    // Build rotation matrix and convert to quaternion
    glm::mat3 rotMatrix;
    rotMatrix[0] = right;
    rotMatrix[1] = actualUp;
    rotMatrix[2] = -forward;  // Vulkan/OpenGL convention: camera looks down -Z

    rotation_ = glm::quat_cast(rotMatrix);
    updateViewMatrix();
}

void Camera::setEulerAngles(float pitch, float yaw, float roll) {
    // Convert degrees to radians
    float pitchRad = glm::radians(pitch);
    float yawRad = glm::radians(yaw);
    float rollRad = glm::radians(roll);

    // Build quaternion from Euler angles using explicit axis-angle rotations
    // This avoids gimbal lock and ensures proper rotation order
    // For FPS camera: Yaw (around Y) * Pitch (around X) * Roll (around Z)
    glm::quat qYaw = glm::angleAxis(yawRad, glm::vec3(0, 1, 0));    // Rotation around world Y-axis
    glm::quat qPitch = glm::angleAxis(pitchRad, glm::vec3(1, 0, 0)); // Rotation around local X-axis
    glm::quat qRoll = glm::angleAxis(rollRad, glm::vec3(0, 0, 1));   // Rotation around local Z-axis

    rotation_ = qYaw * qPitch * qRoll;
    updateViewMatrix();
}

glm::vec3 Camera::eulerAngles() const {
    // Extract Euler angles from forward direction to avoid gimbal lock ambiguity
    // This gives more stable results for FPS-style cameras

    glm::vec3 fwd = forward();

    // Yaw: rotation around Y-axis (horizontal)
    float yaw = glm::degrees(atan2(fwd.x, -fwd.z));

    // Pitch: rotation around X-axis (vertical)
    // Positive pitch = looking up, negative pitch = looking down
    // fwd.y is negative when looking down, positive when looking up
    float pitch = glm::degrees(asin(fwd.y));  // Fixed: removed negation

    // Roll: rotation around Z-axis (for FPS cameras, this should stay near 0)
    // Extract from up vector to determine roll
    glm::vec3 upVec = up();
    glm::vec3 right = glm::cross(fwd, glm::vec3(0, 1, 0));
    if (glm::length(right) > 0.001f) {
        right = glm::normalize(right);
        glm::vec3 expectedUp = glm::cross(right, fwd);
        float roll = glm::degrees(atan2(glm::dot(upVec, right), glm::dot(upVec, expectedUp)));
        return glm::vec3(pitch, yaw, roll);
    }

    return glm::vec3(pitch, yaw, 0.0f);
}

// --- CameraData Population ---

CameraData Camera::toCameraData(uint32_t screenWidth, uint32_t screenHeight) const {
    CameraData data;

    // Matrices
    data.view = viewMatrix_;
    data.projection = projectionMatrix_;
    data.viewProjection = projectionMatrix_ * viewMatrix_;
    data.invView = glm::inverse(viewMatrix_);
    data.invProjection = glm::inverse(projectionMatrix_);

    // Position and direction
    data.position = glm::vec4(position_, 1.0f);
    data.direction = glm::vec4(forward(), 0.0f);

    // Parameters
    data.nearFar = glm::vec2(nearPlane_, farPlane_);
    data.screenSize = glm::vec2(static_cast<float>(screenWidth), static_cast<float>(screenHeight));
    data.fov = fovY_;
    data.aspectRatio = aspectRatio_;

    return data;
}

// --- Internal Update Methods ---

void Camera::updateViewMatrix() {
    // Build view matrix from position and rotation.
    // rotation_ represents the camera orientation from local->world space.
    // View matrix is the inverse of the camera's world transform:
    //   view = R^T * T^{-1}
    glm::mat4 invRotation = glm::mat4_cast(glm::conjugate(rotation_));
    glm::mat4 invTranslation = glm::translate(glm::mat4(1.0f), -position_);
    viewMatrix_ = invRotation * invTranslation;
}

void Camera::updateProjectionMatrix() {
    if (projectionType_ == ProjectionType::Perspective) {
        // Perspective projection
        projectionMatrix_ = glm::perspective(
            glm::radians(fovY_),
            aspectRatio_,
            nearPlane_,
            farPlane_
        );
    } else {
        // Orthographic projection
        float orthoHeight = orthoWidth_ / aspectRatio_;
        projectionMatrix_ = glm::ortho(
            -orthoWidth_ * 0.5f, orthoWidth_ * 0.5f,
            -orthoHeight * 0.5f, orthoHeight * 0.5f,
            nearPlane_,
            farPlane_
        );
    }

    // GLM was designed for OpenGL, where Y coordinate is flipped
    // For Vulkan, we need to flip the Y axis in the projection matrix
    projectionMatrix_[1][1] *= -1.0f;
}

} // namespace hvk
