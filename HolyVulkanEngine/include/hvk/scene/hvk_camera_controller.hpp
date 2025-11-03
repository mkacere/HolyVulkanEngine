#ifndef HVK_CAMERA_CONTROLLER_HPP
#define HVK_CAMERA_CONTROLLER_HPP

#include <hvk/scene/hvk_camera.hpp>
#include <hvk/core/hvk_input.hpp>
#include <hvk/core/hvk_time.hpp>

namespace hvk {

/**
 * CameraController - Behavior class for controlling camera movement
 *
 * Design principles:
 * - Operates on Camera data (doesn't own camera)
 * - Uses Input and Time systems (no direct GLFW dependency)
 * - Supports both FPS-style and orbit camera modes
 * - Smooth movement with acceleration/deceleration
 */
class CameraController {
public:
    enum class Mode {
        FPS,        // First-person shooter style (WASD + mouse look)
        Orbit       // Orbit around target point
    };

    CameraController() = default;

    // --- Configuration ---

    void setMode(Mode mode) { mode_ = mode; }
    Mode mode() const { return mode_; }

    // Movement speeds
    void setMoveSpeed(float speed) { moveSpeed_ = speed; }
    float moveSpeed() const { return moveSpeed_; }

    void setFastMoveMultiplier(float multiplier) { fastMoveMultiplier_ = multiplier; }
    float fastMoveMultiplier() const { return fastMoveMultiplier_; }

    // Mouse sensitivity
    void setMouseSensitivity(float sensitivity) { mouseSensitivity_ = sensitivity; }
    float mouseSensitivity() const { return mouseSensitivity_; }

    // Orbit mode settings
    void setOrbitTarget(const glm::vec3& target) { orbitTarget_ = target; }
    const glm::vec3& orbitTarget() const { return orbitTarget_; }

    void setOrbitDistance(float distance) { orbitDistance_ = distance; }
    float orbitDistance() const { return orbitDistance_; }

    // FPS orientation (useful for initializing controller to match camera's current direction)
    void setYaw(float yawDegrees) { fpsYaw_ = yawDegrees; }
    float yaw() const { return fpsYaw_; }

    void setPitch(float pitchDegrees) { fpsPitch_ = pitchDegrees; }
    float pitch() const { return fpsPitch_; }

    // --- Update ---

    /**
     * Update camera based on input
     * Call once per frame after Input::update()
     *
     * @param camera Camera to control
     */
    void update(Camera& camera);

private:
    // Mode
    Mode mode_ = Mode::FPS;

    // Movement settings
    float moveSpeed_ = 5.0f;              // Units per second
    float fastMoveMultiplier_ = 3.0f;     // Sprint multiplier (when holding Shift)
    float mouseSensitivity_ = 0.1f;       // Degrees per pixel

    // Orbit mode settings
    glm::vec3 orbitTarget_{0.0f, 0.0f, 0.0f};
    float orbitDistance_ = 10.0f;
    float orbitPitch_ = 0.0f;             // Vertical rotation in degrees
    float orbitYaw_ = 0.0f;               // Horizontal rotation in degrees

    // FPS mode state
    float fpsPitch_ = 0.0f;               // Vertical rotation in degrees
    float fpsYaw_ = -90.0f;               // Horizontal rotation in degrees (start looking forward)

    // Internal update methods
    void updateFPS(Camera& camera);
    void updateOrbit(Camera& camera);
};

} // namespace hvk

#endif // HVK_CAMERA_CONTROLLER_HPP
