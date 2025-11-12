#include "pch.h"

#include <hvk/gfx/hvk_window.h>

namespace hvk {

    // -------------- static state for GLFW refcount --------------

    static std::mutex& glfw_refcount_mutex() { static std::mutex m; return m; }
    static int& glfw_refcount() { static int rc = 0; return rc; }

    void Window::s_errorCallback(int code, const char* desc) {
        std::cerr << "[GLFW] Error " << code << ": " << (desc ? desc : "(null)") << std::endl;
    }

    // -------------- Windows helpers --------------

#if defined(_WIN32)
    static void SetPerMonitorDpiV2() {
        HMODULE user = GetModuleHandleA("user32.dll");
        if (!user) return;
        using SetDpiCtxFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT);
        if (auto p = reinterpret_cast<SetDpiCtxFn>(GetProcAddress(user, "SetProcessDpiAwarenessContext"))) {
            p((DPI_AWARENESS_CONTEXT)-4); // PER_MONITOR_AWARE_V2
        }
    }
#endif

    // -------------- GlfwInitGuard --------------

    Window::GlfwInitGuard::GlfwInitGuard() {
        std::lock_guard<std::mutex> lock(glfw_refcount_mutex());
        if (glfw_refcount() == 0) {
            glfwSetErrorCallback(&Window::s_errorCallback);
#if defined(_WIN32)
            SetPerMonitorDpiV2();
            glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WIN32);
#endif
            if (!glfwInit()) throw std::runtime_error("Failed to initialize GLFW.");
            glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
            glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#if defined(_WIN32)
            glfwWindowHint(GLFW_WIN32_KEYBOARD_MENU, GLFW_FALSE);
#endif
        }
        ++glfw_refcount();
    }

    Window::GlfwInitGuard::~GlfwInitGuard() {
        std::lock_guard<std::mutex> lock(glfw_refcount_mutex());
        int& rc = glfw_refcount();
        assert(rc > 0);
        --rc;
        if (rc == 0) glfwTerminate();
    }

    // -------------- WinPerfGuard --------------

#if defined(_WIN32)
    static void SetProcessPowerThrottle(bool disable) {
        // Define only if not already defined by Windows SDK
#ifndef PROCESS_POWER_THROTTLING_CURRENT_VERSION
        typedef struct _PROCESS_POWER_THROTTLING_STATE {
            ULONG Version;
            ULONG ControlMask;
            ULONG StateMask;
        } PROCESS_POWER_THROTTLING_STATE;
#define PROCESS_POWER_THROTTLING_CURRENT_VERSION 1
#endif
#ifndef PROCESS_POWER_THROTTLING_EXECUTION_SPEED
#define PROCESS_POWER_THROTTLING_EXECUTION_SPEED 0x1
#endif

        PROCESS_POWER_THROTTLING_STATE s{};
        s.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
        s.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
        s.StateMask = disable ? 0 : PROCESS_POWER_THROTTLING_EXECUTION_SPEED;

        using SetProcInfoFn = BOOL(WINAPI*)(HANDLE, PROCESS_INFORMATION_CLASS, LPVOID, DWORD);

        // Fix: check the module handle before GetProcAddress
        HMODULE k32 = GetModuleHandleA("kernel32.dll");
        if (!k32) return;

        auto p = reinterpret_cast<SetProcInfoFn>(GetProcAddress(k32, "SetProcessInformation"));
        if (!p) return;

        p(GetCurrentProcess(),
            static_cast<PROCESS_INFORMATION_CLASS>(ProcessPowerThrottling),
            &s,
            sizeof(s));
    }


    Window::WinPerfGuard::WinPerfGuard(const WindowCreateInfo& ci) {
        if (ci.win_timer_granularity_1ms) { if (timeBeginPeriod(1) == TIMERR_NOERROR) timer1ms = true; }
        if (ci.win_disable_power_throttle) { SetProcessPowerThrottle(true); power = true; }
        if (ci.win_high_priority_process) { if (SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS)) prio = true; }
        if (ci.win_prevent_sleep) {
            // Keep system & display awake while the window exists (good for GUI-less engines/tests)
            if (SetThreadExecutionState(ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED)) nosleep = true;
        }
    }
    Window::WinPerfGuard::~WinPerfGuard() {
        if (timer1ms) timeEndPeriod(1);
        if (power)    SetProcessPowerThrottle(false);
        if (prio)     SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
        if (nosleep)  SetThreadExecutionState(ES_CONTINUOUS);
    }
#endif

    // -------------- Window --------------

    Window::Window(const WindowCreateInfo& ci)
        : glfwGuard_()
#if defined(_WIN32)
        , winPerf_(ci)
#endif
    {
        modeRequested_ = ci.mode;
        dtClamp_ = ci.maxDeltaClampSeconds;
        dtAlpha_ = (ci.dtSmoothingAlpha < 0.f ? 0.f : (ci.dtSmoothingAlpha > 1.f ? 1.f : ci.dtSmoothingAlpha));

        createGLFWWindow(ci);
        if (!window_) throw std::runtime_error("Failed to create GLFWwindow.");

        glfwSetWindowUserPointer(window_, this);
        updateCachedSizes();
        installCallbacks();

        if (modeRequested_ == WindowMode::BorderlessFullscreen ||
            (modeRequested_ == WindowMode::Auto &&
#if defined(_WIN32)
                true
#else
                false
#endif
                )) {
            applyBorderlessToPrimaryMonitor();
        }

        // Init timing
        start_ = Clock::now();
        prev_ = start_;
        dt_ = 0.0;
        dtSmoothed_ = 0.0;
        elapsed_ = 0.0;
        frameIndex_ = 0;

        if (ci.visible) glfwShowWindow(window_);
    }

    Window::~Window() {
        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
    }

    Window::Window(Window&& other) noexcept
        : glfwGuard_()
#if defined(_WIN32)
        , winPerf_(*(new WindowCreateInfo{}))
#endif
    {
        *this = std::move(other);
    }

    Window& Window::operator=(Window&& other) noexcept {
        if (this == &other) return *this;

        if (window_) { glfwDestroyWindow(window_); window_ = nullptr; }

        window_ = other.window_;
        cachedWindowSize_ = other.cachedWindowSize_;
        cachedFramebufferSize_ = other.cachedFramebufferSize_;
        contentScaleX_ = other.contentScaleX_;
        contentScaleY_ = other.contentScaleY_;
        framebufferResized_ = other.framebufferResized_;
        callbacks_ = std::move(other.callbacks_);
        modeRequested_ = other.modeRequested_;

        // timing
        start_ = other.start_;
        prev_ = other.prev_;
        dt_ = other.dt_;
        dtSmoothed_ = other.dtSmoothed_;
        elapsed_ = other.elapsed_;
        frameIndex_ = other.frameIndex_;
        dtClamp_ = other.dtClamp_;
        dtAlpha_ = other.dtAlpha_;

        if (window_) {
            glfwSetWindowUserPointer(window_, this);
            installCallbacks();
        }
        other.window_ = nullptr;
        other.framebufferResized_ = false;

        return *this;
    }

    // -------------- Public API --------------

    void Window::poll() {
        glfwPollEvents();
        tickNow();
    }

    void Window::waitAndTick(double timeoutSeconds) {
        if (timeoutSeconds <= 0.0) glfwWaitEvents();
        else                       glfwWaitEventsTimeout(timeoutSeconds);
        tickNow();
    }

    void Window::PollEvents() { glfwPollEvents(); }
    void Window::WaitEvents() { glfwWaitEvents(); }

    bool Window::shouldClose() const {
        return window_ ? glfwWindowShouldClose(window_) == GLFW_TRUE : true;
    }
    void Window::setShouldClose(bool v) {
        if (window_) glfwSetWindowShouldClose(window_, v ? GLFW_TRUE : GLFW_FALSE);
    }

    void Window::setTitle(std::string_view title) {
        if (window_) glfwSetWindowTitle(window_, std::string(title).c_str());
    }

    void Window::show() { if (window_) glfwShowWindow(window_); }
    void Window::hide() { if (window_) glfwHideWindow(window_); }

    ExtentU32 Window::windowSize() const {
        if (!window_) return {};
        int w = 0, h = 0; glfwGetWindowSize(window_, &w, &h);
        return { static_cast<uint32_t>(w > 0 ? w : 0), static_cast<uint32_t>(h > 0 ? h : 0) };
    }

    ExtentU32 Window::framebufferSize() const {
        if (!window_) return {};
        int w = 0, h = 0; glfwGetFramebufferSize(window_, &w, &h);
        return { static_cast<uint32_t>(w > 0 ? w : 0), static_cast<uint32_t>(h > 0 ? h : 0) };
    }

    bool Window::isMinimized() const {
        const auto fb = framebufferSize();
        return fb.width == 0 || fb.height == 0;
    }

    bool Window::wasResized() const { return framebufferResized_; }
    void Window::clearResizedFlag() { framebufferResized_ = false; }

    void Window::contentScale(float& x, float& y) const { x = contentScaleX_; y = contentScaleY_; }

    void Window::centerOnPrimaryMonitor() {
        if (!window_) return;
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        if (!mon) return;
        const GLFWvidmode* mode = glfwGetVideoMode(mon);
        if (!mode) return;
        int mx = 0, my = 0; glfwGetMonitorPos(mon, &mx, &my);
        int ww = 0, wh = 0; glfwGetWindowSize(window_, &ww, &wh);
        const int x = mx + (mode->width - ww) / 2;
        const int y = my + (mode->height - wh) / 2;
        glfwSetWindowPos(window_, x, y);
    }

    // --- Vulkan WSI helpers ---

    std::vector<const char*> Window::requiredVulkanInstanceExtensions() {
        uint32_t count = 0;
        const char** ext = glfwGetRequiredInstanceExtensions(&count);
        if (!ext || count == 0) throw std::runtime_error("GLFW did not report required Vulkan instance extensions.");
        return std::vector<const char*>{ ext, ext + count };
    }

    VulkanWsiBundle14 Window::wsiBundleForVulkan14() {
        VulkanWsiBundle14 b;
        // Always required: ask GLFW for platform WSI + VK_KHR_surface
        b.requiredInstance = requiredVulkanInstanceExtensions();

        // Recommended *instance* extensions for modern WSI plumbing at Vulkan 1.4+
        // (enable only if supported on the target platform)
        b.recommendedInstance.push_back(VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME); // vkGetPhysicalDeviceSurfaceCapabilities2KHR
        // EXT was first; KHR version exists on newer stacks. Your device code can probe both.
        b.recommendedInstance.push_back("VK_KHR_surface_maintenance1"); // promoted from EXT on newer drivers
        // (If you want to also try EXT explicitly in your device code, probe VK_EXT_surface_maintenance1.)

        // Recommended *device* extensions to consider (renderer/swapchain side):
        b.recommendedDevice.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);          // mandatory for presentation
        b.recommendedDevice.push_back("VK_EXT_swapchain_maintenance1");          // per-present mode switching, present fence
        b.recommendedDevice.push_back("VK_KHR_present_id");                      // identify presents for pacing/latency graphs
        b.recommendedDevice.push_back("VK_KHR_present_wait");                    // wait for presentation completion (pacing)
        // On very new stacks you might also probe emerging variants, if you care:
        // b.recommendedDevice.push_back("VK_KHR_present_wait2");
        // b.recommendedDevice.push_back("VK_KHR_present_mode_fifo_latest_ready"); // new FIFO variant optimized for time-based pacing

        return b;
    }

    VkSurfaceKHR Window::createVulkanSurface(VkInstance instance) const {
        if (!window_) throw std::runtime_error("Window is not valid.");
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkResult res = glfwCreateWindowSurface(instance, window_, nullptr, &surface);
        if (res != VK_SUCCESS || surface == VK_NULL_HANDLE) throw std::runtime_error("glfwCreateWindowSurface failed.");
        return surface;
    }

    // -------------- internals --------------

    void Window::installCallbacks() {
        assert(window_);
        glfwSetFramebufferSizeCallback(window_, &Window::s_framebufferSizeCallback);
        glfwSetWindowFocusCallback(window_, &Window::s_windowFocusCallback);
        glfwSetWindowIconifyCallback(window_, &Window::s_windowIconifyCallback);
        glfwSetWindowContentScaleCallback(window_, &Window::s_windowContentScaleCallback);
    }

    void Window::updateCachedSizes() {
        int ww = 0, wh = 0; glfwGetWindowSize(window_, &ww, &wh);
        cachedWindowSize_.width = static_cast<uint32_t>(ww > 0 ? ww : 0);
        cachedWindowSize_.height = static_cast<uint32_t>(wh > 0 ? wh : 0);

        int fw = 0, fh = 0; glfwGetFramebufferSize(window_, &fw, &fh);
        cachedFramebufferSize_.width = static_cast<uint32_t>(fw > 0 ? fw : 0);
        cachedFramebufferSize_.height = static_cast<uint32_t>(fh > 0 ? fh : 0);

        float sx = 1.f, sy = 1.f; glfwGetWindowContentScale(window_, &sx, &sy);
        contentScaleX_ = sx; contentScaleY_ = sy;
    }

    void Window::createGLFWWindow(const WindowCreateInfo& ci) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, ci.visible ? GLFW_TRUE : GLFW_FALSE);

        WindowMode mode = ci.mode;
#if defined(_WIN32)
        if (mode == WindowMode::Auto) mode = WindowMode::BorderlessFullscreen; // fast flip-model path
#else
        if (mode == WindowMode::Auto) mode = WindowMode::Windowed;
#endif
        modeRequested_ = mode;

        if (mode == WindowMode::ExclusiveFullscreen) {
            GLFWmonitor* mon = glfwGetPrimaryMonitor();
            const GLFWvidmode* vm = mon ? glfwGetVideoMode(mon) : nullptr;
            if (!mon || !vm) throw std::runtime_error("No primary monitor for exclusive fullscreen.");
            glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
            glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
            window_ = glfwCreateWindow(vm->width, vm->height, ci.title ? ci.title : "Engine", mon, nullptr);
            return;
        }

        glfwWindowHint(GLFW_RESIZABLE, ci.resizable ? GLFW_TRUE : GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, (mode == WindowMode::BorderlessFullscreen) ? GLFW_FALSE
            : (ci.decorated ? GLFW_TRUE : GLFW_FALSE));
        glfwWindowHint(GLFW_MAXIMIZED, (mode == WindowMode::Windowed && ci.maximized) ? GLFW_TRUE : GLFW_FALSE);

        window_ = glfwCreateWindow(ci.width, ci.height, ci.title ? ci.title : "Engine", nullptr, nullptr);
    }

    void Window::applyBorderlessToPrimaryMonitor() {
        if (!window_) return;
        GLFWmonitor* mon = glfwGetPrimaryMonitor();
        if (!mon) return;
        const GLFWvidmode* vm = glfwGetVideoMode(mon);
        if (!vm) return;

        int mx = 0, my = 0; glfwGetMonitorPos(mon, &mx, &my);
        glfwSetWindowAttrib(window_, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowAttrib(window_, GLFW_RESIZABLE, GLFW_FALSE);
        glfwSetWindowPos(window_, mx, my);
        glfwSetWindowSize(window_, vm->width, vm->height);
        updateCachedSizes();
    }

    // -------------- GLFW callbacks --------------

    void Window::s_framebufferSizeCallback(GLFWwindow* win, int w, int h) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (!self) return;
        self->cachedFramebufferSize_.width = static_cast<uint32_t>(w > 0 ? w : 0);
        self->cachedFramebufferSize_.height = static_cast<uint32_t>(h > 0 ? h : 0);
        self->framebufferResized_ = true;
        if (self->callbacks_.onFramebufferResized) {
            self->callbacks_.onFramebufferResized(self->cachedFramebufferSize_.width,
                self->cachedFramebufferSize_.height);
        }
    }

    void Window::s_windowFocusCallback(GLFWwindow* win, int focused) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (!self) return;
        if (self->callbacks_.onFocusChanged) self->callbacks_.onFocusChanged(focused == GLFW_TRUE);
    }

    void Window::s_windowIconifyCallback(GLFWwindow* win, int iconified) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (!self) return;
        if (self->callbacks_.onIconifyChanged) self->callbacks_.onIconifyChanged(iconified == GLFW_TRUE);
    }

    void Window::s_windowContentScaleCallback(GLFWwindow* win, float xscale, float yscale) {
        auto* self = static_cast<Window*>(glfwGetWindowUserPointer(win));
        if (!self) return;
        self->contentScaleX_ = xscale;
        self->contentScaleY_ = yscale;
        if (self->callbacks_.onContentScaleChanged) self->callbacks_.onContentScaleChanged(xscale, yscale);
    }

    // -------------- Delta time core --------------

    void Window::tickNow() {
        const auto now = Clock::now();
        const std::chrono::duration<double> raw = now - prev_;

        double dt = raw.count();
        if (dtClamp_ > 0.0 && dt > dtClamp_) dt = dtClamp_; // clamp spikes

        dt_ = dt;
        if (dtAlpha_ > 0.f) {
            if (frameIndex_ == 0) dtSmoothed_ = dt_; // seed
            else dtSmoothed_ = (1.0 - dtAlpha_) * dtSmoothed_ + dtAlpha_ * dt_;
        }
        else {
            dtSmoothed_ = dt_;
        }

        elapsed_ = std::chrono::duration<double>(now - start_).count();
        prev_ = now;
        ++frameIndex_;
    }

} // namespace hvk
