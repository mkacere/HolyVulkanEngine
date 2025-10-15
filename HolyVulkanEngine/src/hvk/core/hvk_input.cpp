#include <hvk/core/hvk_input.hpp>

namespace hvk {

// --- Static member initialization ---
GLFWwindow* Input::window_ = nullptr;
std::unordered_map<int, bool> Input::keyState_;
std::unordered_map<int, bool> Input::prevKeyState_;
std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> Input::mouseButtonState_{};
std::array<bool, GLFW_MOUSE_BUTTON_LAST + 1> Input::prevMouseButtonState_{};
glm::vec2 Input::mousePos_{0.0f, 0.0f};
glm::vec2 Input::prevMousePos_{0.0f, 0.0f};
glm::vec2 Input::mouseDelta_{0.0f, 0.0f};
glm::vec2 Input::scrollDelta_{0.0f, 0.0f};
int Input::cursorMode_ = GLFW_CURSOR_NORMAL;
bool Input::firstFrame_ = true;

// --- Initialization ---

void Input::init(GLFWwindow* window) {
    window_ = window;
    firstFrame_ = true;

    // Get initial mouse position
    double x, y;
    glfwGetCursorPos(window_, &x, &y);
    mousePos_ = glm::vec2(static_cast<float>(x), static_cast<float>(y));
    prevMousePos_ = mousePos_;

    // Set up scroll callback
    glfwSetScrollCallback(window_, scrollCallback);
}

void Input::cleanup() {
    window_ = nullptr;
    keyState_.clear();
    prevKeyState_.clear();
    mouseButtonState_.fill(false);
    prevMouseButtonState_.fill(false);
}

// --- Update ---

void Input::update() {
    if (!window_) return;

    // Update previous key states
    prevKeyState_ = keyState_;

    // Poll all commonly used keys
    // (You can expand this list or use a different approach if needed)
    static const int commonKeys[] = {
        GLFW_KEY_W, GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_D,
        GLFW_KEY_Q, GLFW_KEY_E, GLFW_KEY_R, GLFW_KEY_F,
        GLFW_KEY_SPACE, GLFW_KEY_LEFT_SHIFT, GLFW_KEY_LEFT_CONTROL, GLFW_KEY_LEFT_ALT,
        GLFW_KEY_ESCAPE, GLFW_KEY_ENTER, GLFW_KEY_TAB,
        GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT,
        GLFW_KEY_0, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3, GLFW_KEY_4,
        GLFW_KEY_5, GLFW_KEY_6, GLFW_KEY_7, GLFW_KEY_8, GLFW_KEY_9,
    };

    for (int key : commonKeys) {
        int state = glfwGetKey(window_, key);
        keyState_[key] = (state == GLFW_PRESS || state == GLFW_REPEAT);
    }

    // Update previous mouse button states
    prevMouseButtonState_ = mouseButtonState_;

    // Poll mouse buttons
    for (int button = 0; button <= GLFW_MOUSE_BUTTON_LAST; ++button) {
        int state = glfwGetMouseButton(window_, button);
        mouseButtonState_[button] = (state == GLFW_PRESS);
    }

    // Update mouse position and delta
    prevMousePos_ = mousePos_;
    double x, y;
    glfwGetCursorPos(window_, &x, &y);
    mousePos_ = glm::vec2(static_cast<float>(x), static_cast<float>(y));

    if (firstFrame_) {
        // Prevent huge delta on first frame
        mouseDelta_ = glm::vec2(0.0f, 0.0f);
        firstFrame_ = false;
    } else {
        mouseDelta_ = mousePos_ - prevMousePos_;
    }

    // Scroll delta is reset to zero each frame (only non-zero if callback was triggered)
    // The callback accumulates scroll events, but we reset here for next frame
    // Note: scrollDelta_ is set by the callback
}

// --- Keyboard ---

bool Input::isKeyPressed(int key) {
    auto it = keyState_.find(key);
    return it != keyState_.end() && it->second;
}

bool Input::wasKeyJustPressed(int key) {
    auto curIt = keyState_.find(key);
    auto prevIt = prevKeyState_.find(key);

    bool current = (curIt != keyState_.end() && curIt->second);
    bool previous = (prevIt != prevKeyState_.end() && prevIt->second);

    return current && !previous;
}

bool Input::wasKeyJustReleased(int key) {
    auto curIt = keyState_.find(key);
    auto prevIt = prevKeyState_.find(key);

    bool current = (curIt != keyState_.end() && curIt->second);
    bool previous = (prevIt != prevKeyState_.end() && prevIt->second);

    return !current && previous;
}

// --- Mouse Buttons ---

bool Input::isMouseButtonPressed(int button) {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return mouseButtonState_[button];
}

bool Input::wasMouseButtonJustPressed(int button) {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return mouseButtonState_[button] && !prevMouseButtonState_[button];
}

bool Input::wasMouseButtonJustReleased(int button) {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) return false;
    return !mouseButtonState_[button] && prevMouseButtonState_[button];
}

// --- Mouse Position and Delta ---

glm::vec2 Input::mousePosition() {
    return mousePos_;
}

glm::vec2 Input::mouseDelta() {
    return mouseDelta_;
}

glm::vec2 Input::scrollDelta() {
    return scrollDelta_;
}

// --- Cursor Mode ---

void Input::setCursorMode(int mode) {
    if (!window_) return;
    cursorMode_ = mode;
    glfwSetInputMode(window_, GLFW_CURSOR, mode);

    // Reset first frame flag to prevent huge delta when switching modes
    firstFrame_ = true;
}

bool Input::isCursorDisabled() {
    return cursorMode_ == GLFW_CURSOR_DISABLED;
}

// --- GLFW Callbacks ---

void Input::scrollCallback(GLFWwindow* window, double xoffset, double yoffset) {
    // Accumulate scroll delta (will be reset in update())
    scrollDelta_ += glm::vec2(static_cast<float>(xoffset), static_cast<float>(yoffset));
}

} // namespace hvk
