#include <hvk/scene/hvk_camera_controller.hpp>
#include <algorithm>

namespace hvk {

void CameraController::update(Camera& camera) {
    if (mode_ == Mode::FPS) {
        updateFPS(camera);
    } else {
        updateOrbit(camera);
    }
}

void CameraController::updateFPS(Camera& camera) {
    float dt = Time::deltaTime();

    // On first update, initialize controller from camera's current orientation
    // This allows the camera to be positioned/rotated before the controller takes over
    if (!fpsInitialized_) {
        glm::vec3 euler = camera.eulerAngles();
        fpsPitch_ = euler.x;  // pitch
        fpsYaw_ = euler.y;    // yaw
        fpsInitialized_ = true;

        // Lock in the euler representation immediately to prevent snap on first mouse movement
        // This ensures the camera uses euler-based rotation from frame 1, so mouse deltas
        // are applied smoothly. Now that the pitch sign is fixed, this preserves the rotation.
        camera.setEulerAngles(fpsPitch_, fpsYaw_, 0.0f);
    }

    // Track if rotation changed this frame
    bool rotationChanged = false;

    // Mouse look (only if cursor is disabled)
    if (Input::isCursorDisabled()) {
        glm::vec2 mouseDelta = Input::mouseDelta();

        // Only update if mouse actually moved
        if (glm::length(mouseDelta) > 0.001f) {
            // Update yaw and pitch
            fpsYaw_ -= mouseDelta.x * mouseSensitivity_;
            fpsPitch_ -= mouseDelta.y * mouseSensitivity_;

            // Clamp pitch to prevent camera flipping
            fpsPitch_ = std::clamp(fpsPitch_, -89.0f, 89.0f);

            rotationChanged = true;
        }
    }

    // Only update camera rotation if it changed
    if (rotationChanged) {
        camera.setEulerAngles(fpsPitch_, fpsYaw_, 0.0f);
    }

    // WASD movement
    glm::vec3 movement(0.0f);

    if (Input::isKeyPressed(GLFW_KEY_W)) {
        movement += camera.forward();
    }
    if (Input::isKeyPressed(GLFW_KEY_S)) {
        movement -= camera.forward();
    }
    if (Input::isKeyPressed(GLFW_KEY_A)) {
        movement -= camera.right();
    }
    if (Input::isKeyPressed(GLFW_KEY_D)) {
        movement += camera.right();
    }

    // Up/down movement (Q/E or Space/Ctrl)
    if (Input::isKeyPressed(GLFW_KEY_SPACE)) {
        movement += glm::vec3(0.0f, 1.0f, 0.0f);
    }
    if (Input::isKeyPressed(GLFW_KEY_LEFT_CONTROL)) {
        movement -= glm::vec3(0.0f, 1.0f, 0.0f);
    }

    // Normalize movement vector
    if (glm::length(movement) > 0.001f) {
        movement = glm::normalize(movement);
    }

    // Apply movement speed
    float speed = moveSpeed_;
    if (Input::isKeyPressed(GLFW_KEY_LEFT_SHIFT)) {
        speed *= fastMoveMultiplier_;
    }

    // Update camera position
    camera.setPosition(camera.position() + movement * speed * dt);
}

void CameraController::updateOrbit(Camera& camera) {
    // Mouse rotation
    if (Input::isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        glm::vec2 mouseDelta = Input::mouseDelta();

        orbitYaw_ -= mouseDelta.x * mouseSensitivity_;
        orbitPitch_ -= mouseDelta.y * mouseSensitivity_;

        // Clamp pitch
        orbitPitch_ = std::clamp(orbitPitch_, -89.0f, 89.0f);
    }

    // Zoom with scroll wheel
    glm::vec2 scroll = Input::scrollDelta();
    orbitDistance_ -= scroll.y * 0.5f;
    orbitDistance_ = std::max(orbitDistance_, 0.5f);  // Minimum distance

    // Calculate camera position from orbit parameters
    float pitchRad = glm::radians(orbitPitch_);
    float yawRad = glm::radians(orbitYaw_);

    glm::vec3 offset;
    offset.x = orbitDistance_ * cos(pitchRad) * cos(yawRad);
    offset.y = orbitDistance_ * sin(pitchRad);
    offset.z = orbitDistance_ * cos(pitchRad) * sin(yawRad);

    camera.setPosition(orbitTarget_ + offset);
    camera.lookAt(orbitTarget_);
}

} // namespace hvk
