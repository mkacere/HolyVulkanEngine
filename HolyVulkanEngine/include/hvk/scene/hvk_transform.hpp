#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>

namespace hvk {

/**
 * @brief Transform - Position, Rotation, Scale utility class
 *
 * Provides a convenient way to work with 3D transformations using separate
 * position, rotation (quaternion), and scale components. Automatically computes
 * the final transformation matrix when needed.
 *
 * This is useful for game objects, cameras, and scene nodes before transitioning
 * to a full ECS system.
 */
class Transform {
public:
    // ========================================================================
    // Constructors
    // ========================================================================

    Transform() = default;

    explicit Transform(const glm::vec3& position)
        : position_(position) {}

    Transform(const glm::vec3& position, const glm::quat& rotation)
        : position_(position), rotation_(rotation) {}

    Transform(const glm::vec3& position, const glm::quat& rotation, const glm::vec3& scale)
        : position_(position), rotation_(rotation), scale_(scale) {}

    // ========================================================================
    // Position
    // ========================================================================

    const glm::vec3& position() const { return position_; }
    void setPosition(const glm::vec3& pos) { position_ = pos; dirty_ = true; }

    void translate(const glm::vec3& delta) { position_ += delta; dirty_ = true; }

    float x() const { return position_.x; }
    float y() const { return position_.y; }
    float z() const { return position_.z; }

    void setX(float x) { position_.x = x; dirty_ = true; }
    void setY(float y) { position_.y = y; dirty_ = true; }
    void setZ(float z) { position_.z = z; dirty_ = true; }

    // ========================================================================
    // Rotation (Quaternion)
    // ========================================================================

    const glm::quat& rotation() const { return rotation_; }
    void setRotation(const glm::quat& rot) { rotation_ = rot; dirty_ = true; }

    // Set rotation from Euler angles (in radians)
    void setEulerAngles(const glm::vec3& eulerRadians) {
        rotation_ = glm::quat(eulerRadians);
        dirty_ = true;
    }

    // Set rotation from Euler angles (in degrees, more intuitive)
    void setEulerDegrees(const glm::vec3& eulerDegrees) {
        rotation_ = glm::quat(glm::radians(eulerDegrees));
        dirty_ = true;
    }

    // Get Euler angles (in radians)
    glm::vec3 eulerAngles() const {
        return glm::eulerAngles(rotation_);
    }

    // Get Euler angles (in degrees)
    glm::vec3 eulerDegrees() const {
        return glm::degrees(glm::eulerAngles(rotation_));
    }

    // Rotate by axis-angle (axis should be normalized, angle in radians)
    void rotate(const glm::vec3& axis, float angleRadians) {
        rotation_ = glm::angleAxis(angleRadians, axis) * rotation_;
        dirty_ = true;
    }

    // Rotate by quaternion
    void rotate(const glm::quat& delta) {
        rotation_ = delta * rotation_;
        dirty_ = true;
    }

    // Rotate around X axis (radians)
    void rotateX(float angleRadians) {
        rotate(glm::vec3(1, 0, 0), angleRadians);
    }

    // Rotate around Y axis (radians)
    void rotateY(float angleRadians) {
        rotate(glm::vec3(0, 1, 0), angleRadians);
    }

    // Rotate around Z axis (radians)
    void rotateZ(float angleRadians) {
        rotate(glm::vec3(0, 0, 1), angleRadians);
    }

    // ========================================================================
    // Scale
    // ========================================================================

    const glm::vec3& scale() const { return scale_; }
    void setScale(const glm::vec3& s) { scale_ = s; dirty_ = true; }
    void setScale(float uniformScale) { scale_ = glm::vec3(uniformScale); dirty_ = true; }

    void scaleBy(const glm::vec3& factor) { scale_ *= factor; dirty_ = true; }
    void scaleBy(float factor) { scale_ *= factor; dirty_ = true; }

    // ========================================================================
    // Direction Vectors
    // ========================================================================

    // Get forward vector (object's local -Z axis in world space)
    glm::vec3 forward() const {
        return rotation_ * glm::vec3(0, 0, -1);
    }

    // Get right vector (object's local +X axis in world space)
    glm::vec3 right() const {
        return rotation_ * glm::vec3(1, 0, 0);
    }

    // Get up vector (object's local +Y axis in world space)
    glm::vec3 up() const {
        return rotation_ * glm::vec3(0, 1, 0);
    }

    // ========================================================================
    // Matrix Computation
    // ========================================================================

    // Get the final transformation matrix (TRS order: Translate * Rotate * Scale)
    const glm::mat4& matrix() const {
        if (dirty_) {
            updateMatrix();
        }
        return matrix_;
    }

    // Manually set the transformation matrix (extracts position, rotation, scale)
    void setMatrix(const glm::mat4& mat) {
        // Extract translation
        position_ = glm::vec3(mat[3]);

        // Extract scale
        glm::vec3 scaleX = glm::vec3(mat[0]);
        glm::vec3 scaleY = glm::vec3(mat[1]);
        glm::vec3 scaleZ = glm::vec3(mat[2]);
        scale_ = glm::vec3(glm::length(scaleX), glm::length(scaleY), glm::length(scaleZ));

        // Extract rotation (normalize the basis vectors)
        glm::mat3 rotationMatrix;
        rotationMatrix[0] = scaleX / scale_.x;
        rotationMatrix[1] = scaleY / scale_.y;
        rotationMatrix[2] = scaleZ / scale_.z;
        rotation_ = glm::quat_cast(rotationMatrix);

        matrix_ = mat;
        dirty_ = false;
    }

    // ========================================================================
    // Look-At Utilities
    // ========================================================================

    // Make the transform look at a target point (up vector defaults to +Y)
    void lookAt(const glm::vec3& target, const glm::vec3& worldUp = glm::vec3(0, 1, 0)) {
        glm::vec3 direction = glm::normalize(target - position_);

        // Handle degenerate case where direction is parallel to worldUp
        if (glm::abs(glm::dot(direction, worldUp)) > 0.999f) {
            // Choose a different up vector
            glm::vec3 altUp = glm::abs(worldUp.y) < 0.999f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
            rotation_ = glm::quatLookAt(direction, altUp);
        } else {
            rotation_ = glm::quatLookAt(direction, worldUp);
        }

        dirty_ = true;
    }

    // ========================================================================
    // Interpolation
    // ========================================================================

    // Linear interpolation between two transforms
    static Transform lerp(const Transform& a, const Transform& b, float t) {
        Transform result;
        result.position_ = glm::mix(a.position_, b.position_, t);
        result.rotation_ = glm::slerp(a.rotation_, b.rotation_, t); // spherical interpolation
        result.scale_ = glm::mix(a.scale_, b.scale_, t);
        result.dirty_ = true;
        return result;
    }

    // Smooth interpolation (hermite)
    static Transform smoothstep(const Transform& a, const Transform& b, float t) {
        float smooth = t * t * (3.0f - 2.0f * t);
        return lerp(a, b, smooth);
    }

    // ========================================================================
    // Reset
    // ========================================================================

    void reset() {
        position_ = glm::vec3(0.0f);
        rotation_ = glm::quat(1, 0, 0, 0); // identity quaternion
        scale_ = glm::vec3(1.0f);
        dirty_ = true;
    }

private:
    void updateMatrix() const {
        // TRS order: Translate * Rotate * Scale
        glm::mat4 T = glm::translate(glm::mat4(1.0f), position_);
        glm::mat4 R = glm::mat4_cast(rotation_);
        glm::mat4 S = glm::scale(glm::mat4(1.0f), scale_);
        matrix_ = T * R * S;
        dirty_ = false;
    }

private:
    glm::vec3 position_ = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::quat rotation_ = glm::quat(1, 0, 0, 0); // identity (w, x, y, z)
    glm::vec3 scale_ = glm::vec3(1.0f, 1.0f, 1.0f);

    mutable glm::mat4 matrix_ = glm::mat4(1.0f);
    mutable bool dirty_ = true;
};

} // namespace hvk
