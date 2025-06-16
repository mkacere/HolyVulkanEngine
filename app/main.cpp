#include "hvk_window.h"
#include "hvk_device.h"
#include "hvk_renderer.h"
#include "hvk_camera.h"
#include "hvk_game_object.h"
#include "hvk_global_ubo.hpp"
#include "systems/hvk_model_render_system.h"

#include <GLFW/glfw3.h>
#include <memory>

// Simple mouse‐move callback that forwards to your HvkCamera
static void mouseCallback(GLFWwindow* win, double xpos, double ypos) {
    auto cam = reinterpret_cast<hvk::HvkCamera*>(glfwGetWindowUserPointer(win));
    cam->processMouseMovement(xpos, ypos);
}

int main() {
    // 1) Create window & device
    hvk::HvkWindow window{ 800, 600, "My Vulkan App" };
    hvk::HvkDevice device{ window };

    // 2) Set up the camera (pass in the GLFWwindow* so it can hook callbacks)
    float aspect = window.getExtent().width / float(window.getExtent().height);
    hvk::HvkCamera camera{
        window.getGlfwWindow(),     // raw GLFW handle
        glm::radians(60.0f),        // vertical FOV
        aspect,                     // aspect ratio
        0.1f,                       // near plane
        100.0f                      // far plane
    };

    // tell GLFW to forward mouse movements to our camera
    glfwSetWindowUserPointer(window.getGlfwWindow(), &camera);
    glfwSetCursorPosCallback(window.getGlfwWindow(), mouseCallback);
    // hide the cursor and capture it for FPS‐style look
    glfwSetInputMode(window.getGlfwWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // 3) Create renderer and add our ModelRenderSystem
    hvk::HvkRenderer renderer{ window, device };
    renderer.addRenderSystem(
        std::make_unique<hvk::ModelRenderSystem>(
            device,
            "../../../../assets/models/Crystar_Kokoro_Fudoji.glb"
        )
    );

    // optional placeholder for future passes
    hvk::HvkGameObject::Map gameObjects;

    // 4) Main loop: track both absolute time (for animation) and deltaTime (for the camera)
    float lastTime = static_cast<float>(glfwGetTime());
    while (!window.shouldClose()) {
        glfwPollEvents();

        // compute timing
        float currentTime = static_cast<float>(glfwGetTime());
        float deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        // update camera (process WASD, etc.)
        camera.update(deltaTime);

        // render: we pass currentTime as our “frameTime” so systems can animate
        renderer.drawFrame(currentTime, camera, gameObjects);
    }

    vkDeviceWaitIdle(device.device());
    return 0;
}
