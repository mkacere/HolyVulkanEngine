/**
 * @file hvk_input.hpp
 * @brief Centralized input management system for keyboard and mouse input
 * @author Holy Vulkan Engine
 * @date 2025
 */

#ifndef HVK_INPUT_HPP
#define HVK_INPUT_HPP

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <array>
#include <unordered_map>

namespace hvk {

/**
 * Input - Centralized input state tracker
 *
 * Design principles:
 * - Poll GLFW state once per frame (at start of frame)
 * - All systems query Input instead of calling GLFW directly
 * - Provides delta values for mouse movement
 * - Frame-based input detection (pressed this frame, released this frame)
 * - Thread-safe: Only update() should be called from main thread
 *
 * Usage:
 *   Input::init(window);
 *
 *   // In main loop:
 *   Input::update();
 *
 *   // In systems:
 *   if (Input::isKeyPressed(GLFW_KEY_W)) { ... }
 *   if (Input::wasKeyJustPressed(GLFW_KEY_SPACE)) { ... }
 *   glm::vec2 mouseDelta = Input::mouseDelta();
 */
class Input {
public:
    // --- Initialization ---

    /**
     * Initialize input system with GLFW window
     *
     * @param window GLFW window handle
     */
    static void init(GLFWwindow* window);

    /**
     * Update input state (call once per frame at start of frame)
     * Polls GLFW for current key/mouse state and computes deltas
     */
    static void update();

    /**
     * Cleanup input system
     */
    static void cleanup();

    // --- Keyboard ---

    /**
     * Check if key is currently pressed
     *
     * @param key GLFW key code (e.g., GLFW_KEY_W)
     * @return true if key is currently down
     */
    static bool isKeyPressed(int key);

    /**
     * Check if key was just pressed this frame
     *
     * @param key GLFW key code
     * @return true if key transitioned from up to down this frame
     */
    static bool wasKeyJustPressed(int key);

    /**
     * Check if key was just released this frame
     *
     * @param key GLFW key code
     * @return true if key transitioned from down to up this frame
     */
    static bool wasKeyJustReleased(int key);

    // --- Mouse Buttons ---

    /**
     * Check if mouse button is currently pressed
     *
     * @param button GLFW mouse button code (e.g., GLFW_MOUSE_BUTTON_LEFT)
     * @return true if button is currently down
     */
    static bool isMouseButtonPressed(int button);

    /**
     * Check if mouse button was just pressed this frame
     *
     * @param button GLFW mouse button code
     * @return true if button transitioned from up to down this frame
     */
    static bool wasMouseButtonJustPressed(int button);

    /**
     * Check if mouse button was just released this frame
     *
     * @param button GLFW mouse button code
     * @return true if button transitioned from down to up this frame
     */
    static bool wasMouseButtonJustReleased(int button);

    // --- Mouse Position and Delta ---

    /**
     * Get current mouse position in screen coordinates
     *
     * @return (x, y) position where (0,0) is top-left
     */
    static glm::vec2 mousePosition();

    /**
     * Get mouse movement delta since last frame
     *
     * @return (dx, dy) movement in pixels
     */
    static glm::vec2 mouseDelta();

    /**
     * Get mouse scroll wheel delta since last frame
     *
     * @return (x_offset, y_offset) where y_offset is typical vertical scroll
     */
    static glm::vec2 scrollDelta();

    // --- Cursor Mode ---

    /**
     * Set cursor mode
     *
     * @param mode GLFW cursor mode:
     *   GLFW_CURSOR_NORMAL: Cursor visible and behaves normally
     *   GLFW_CURSOR_HIDDEN: Cursor invisible but behaves normally
     *   GLFW_CURSOR_DISABLED: Cursor hidden and locked for FPS-style camera
     */
    static void setCursorMode(int mode);

    /**
     * Check if cursor is disabled (locked for FPS camera)
     */
    static bool isCursorDisabled();

private:
    // GLFW window handle
    static GLFWwindow* window_;

    // Keyboard state (current and previous frame)
    static std::unordered_map<int, bool> keyState_;
    static std::unordered_map<int, bool> prevKeyState_;

    // Mouse button state (current and previous frame)
    static std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> mouseButtonState_;
    static std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> prevMouseButtonState_;

    // Mouse position
    static glm::vec2 mousePos_;
    static glm::vec2 prevMousePos_;
    static glm::vec2 mouseDelta_;

    // Scroll wheel
    static glm::vec2 scrollDelta_;

    // Cursor mode
    static int cursorMode_;

    // First frame flag (to prevent huge mouse delta on first frame)
    static bool firstFrame_;

    // GLFW callbacks
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);

    // Prevent instantiation
    Input() = delete;
};

} // namespace hvk

#endif // HVK_INPUT_HPP
