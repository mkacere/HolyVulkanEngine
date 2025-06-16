#ifndef HVK_CAMERA
#define HVK_CAMERA

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

namespace hvk {
    class HvkCamera {
    public:
        HvkCamera(GLFWwindow* window, float fovY, float aspect, float nearPlane, float farPlane);
        ~HvkCamera() = default;

        void update(float deltaTime);
        void processMouseMovement(double xpos, double ypos);
        void processKeyboard(float deltaTime);

        void setOrthographicProjection(float left, float right, float top, float bottom, float nearPlane, float farPlane);
        void setPerspectiveProjection(float fovY, float aspect, float nearPlane, float farPlane);

        const glm::mat4& getProjection() const { return projectionMatrix_; }
        const glm::mat4& getView() const { return viewMatrix_; }
        const glm::mat4& getInverseView() const { return inverseViewMatrix_; }
        glm::vec3 getPosition() const { return position_; }

    private:
        void updateViewMatrix();

        GLFWwindow* window_;
        glm::mat4 projectionMatrix_{ 1.f };
        glm::mat4 viewMatrix_{ 1.f };
        glm::mat4 inverseViewMatrix_{ 1.f };

        // Camera attributes
        glm::vec3 position_{ 0.f, 0.f, 3.f };
        float yaw_{ -90.f };
        float pitch_{ 0.f };
        glm::vec3 front_{ 0.f, 0.f, -1.f };
        glm::vec3 up_{ 0.f, 1.f, 0.f };
        glm::vec3 right_{ 1.f, 0.f, 0.f };
        glm::vec3 worldUp_{ 0.f, 1.f, 0.f };

        // Options
        float movementSpeed_{ 2.5f };
        float mouseSensitivity_{ 0.1f };

        // Mouse state
        bool firstMouse_{ true };
        double lastX_{ 0 }, lastY_{ 0 };

        void handleInput(float deltaTime);
    };
}

#endif // HVK_CAMERA