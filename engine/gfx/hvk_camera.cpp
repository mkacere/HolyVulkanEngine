#include "hvk_camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace hvk {

    HvkCamera::HvkCamera(GLFWwindow* window, float fovY, float aspect, float nearPlane, float farPlane)
        : window_(window)
    {
        setPerspectiveProjection(fovY, aspect, nearPlane, farPlane);
        glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        double x, y;
        glfwGetCursorPos(window_, &x, &y);
        lastX_ = x;
        lastY_ = y;
        updateViewMatrix();
    }

    void HvkCamera::setOrthographicProjection(float left, float right, float top, float bottom, float nearPlane, float farPlane) {
        projectionMatrix_ = glm::ortho(left, right, bottom, top, nearPlane, farPlane);
        projectionMatrix_[1][1] *= -1.f;
    }

    void HvkCamera::setPerspectiveProjection(float fovY, float aspect, float nearPlane, float farPlane) {
        projectionMatrix_ = glm::perspective(glm::radians(fovY), aspect, nearPlane, farPlane);
        projectionMatrix_[1][1] *= -1; // Vulkan flip
    }

    void HvkCamera::update(float deltaTime) {
        handleInput(deltaTime);
        updateViewMatrix();
    }

    void HvkCamera::handleInput(float deltaTime) {
        processKeyboard(deltaTime);
        double xpos, ypos;
        glfwGetCursorPos(window_, &xpos, &ypos);
        processMouseMovement(xpos, ypos);
    }

    void HvkCamera::processKeyboard(float deltaTime) {
        float velocity = movementSpeed_ * deltaTime;
        if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS)
            position_ += front_ * velocity;
        if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS)
            position_ -= front_ * velocity;
        if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS)
            position_ -= right_ * velocity;
        if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS)
            position_ += right_ * velocity;
    }

    void HvkCamera::processMouseMovement(double xpos, double ypos) {
        if (firstMouse_) {
            lastX_ = xpos;
            lastY_ = ypos;
            firstMouse_ = false;
        }
        float xoffset = float(xpos - lastX_);
        float yoffset = float(lastY_ - ypos);
        lastX_ = xpos;
        lastY_ = ypos;

        xoffset *= mouseSensitivity_;
        yoffset *= mouseSensitivity_;

        yaw_ += xoffset;
        pitch_ += yoffset;

        if (pitch_ > 89.0f) pitch_ = 89.0f;
        if (pitch_ < -89.0f) pitch_ = -89.0f;

        // update Front, Right and Up Vectors using the updated Euler angles
        glm::vec3 front;
        front.x = cos(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        front.y = sin(glm::radians(pitch_));
        front.z = sin(glm::radians(yaw_)) * cos(glm::radians(pitch_));
        front_ = glm::normalize(front);
        right_ = glm::normalize(glm::cross(front_, worldUp_));
        up_ = glm::normalize(glm::cross(right_, front_));
    }

    void HvkCamera::updateViewMatrix() {
        viewMatrix_ = glm::lookAt(position_, position_ + front_, up_);
        inverseViewMatrix_ = glm::inverse(viewMatrix_);
    }

} // namespace hvk