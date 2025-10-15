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

    // Mouse look (only if cursor is disabled)
    if (Input::isCursorDisabled()) {
        glm::vec2 mouseDelta = Input::mouseDelta();

        // Update yaw and pitch
        fpsYaw_ -= mouseDelta.x * mouseSensitivity_;
        fpsPitch_ -= mouseDelta.y * mouseSensitivity_;

        // Clamp pitch to prevent camera flipping
        fpsPitch_ = std::clamp(fpsPitch_, -89.0f, 89.0f);
    }

    // Set camera rotation from pitch and yaw
    camera.setEulerAngles(fpsPitch_, fpsYaw_, 0.0f);

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
