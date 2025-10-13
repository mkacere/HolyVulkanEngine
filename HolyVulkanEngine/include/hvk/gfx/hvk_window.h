#ifndef HVK_WINDOW
#define HVK_WINDOW

// GLFW + Vulkan window tailored for a Vulkan 1.4 engine runtime.
// - OOP/RAII, move-only
// - Windows fast path (borderless fullscreen, DPIv2, timer & power hints)
// - Built-in delta-time tracking (poll/wait update dt)
// - NEW: WSI bundle helper for Vulkan 1.4-era best practices
// - NEW: Optional Windows sleep-prevention during gameplay

#if defined(_WIN32)
#define VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <stdexcept>
#include <mutex>
#include <chrono>

#if defined(_WIN32)
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <timeapi.h>
#endif

namespace hvk {

    enum class WindowMode {
        Auto,
        Windowed,
        BorderlessFullscreen,
        ExclusiveFullscreen
    };

    struct WindowCreateInfo {
        int         width = 1280;
        int         height = 720;
        const char* title = "Engine";
        WindowMode  mode = WindowMode::Auto;
        bool        visible = true;
        bool        resizable = true;
        bool        decorated = true;
        bool        maximized = false;

        // Windows-only runtime hints (safe defaults)
        bool        win_timer_granularity_1ms = true;   // timeBeginPeriod(1)
        bool        win_disable_power_throttle = true;  // PROCESS_POWER_THROTTLING_EXECUTION_SPEED
        bool        win_high_priority_process = false; // HIGH_PRIORITY_CLASS
        bool        win_prevent_sleep = true;  // keep system/display awake while window lives

        // Delta-time options
        double      maxDeltaClampSeconds = 0.25;  // clamp spikes (alt-tab, breakpoints)
        float       dtSmoothingAlpha = 0.0f;  // 0=off; EMA alpha [0..1]
    };

    struct ExtentU32 {
        uint32_t width = 0;
        uint32_t height = 0;
    };

    // A small helper describing which WSI-related extensions are worth enabling
    // at Vulkan 1.4+ if the platform supports them. Pass these to your Device
    // *optionally*; they're not hard requirements.
    struct VulkanWsiBundle14 {
        std::vector<const char*> requiredInstance;    // from GLFW (VK_KHR_surface + platform surface)
        std::vector<const char*> recommendedInstance; // e.g., VK_KHR_get_surface_capabilities2, VK_KHR_surface_maintenance1
        std::vector<const char*> recommendedDevice;   // e.g., VK_EXT_swapchain_maintenance1, VK_KHR_present_id/wait
    };

    class Window {
    public:
        struct Callbacks {
            std::function<void(uint32_t, uint32_t)> onFramebufferResized;
            std::function<void(bool)>               onFocusChanged;
            std::function<void(bool)>               onIconifyChanged;
            std::function<void(float, float)>       onContentScaleChanged;
        };

    public:
        explicit Window(const WindowCreateInfo& ci);
        ~Window();

        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&& other) noexcept;
        Window& operator=(Window&& other) noexcept;

        // Event pump (instance versions update dt)
        void  poll();                               // glfwPollEvents + tick dt
        void  waitAndTick(double timeoutSeconds = 0); // glfwWaitEvents(Timeout) + tick dt

        // Static fallbacks (do not update dt)
        static void PollEvents();
        static void WaitEvents();

        // Lifecycle / status
        bool shouldClose() const;
        void setShouldClose(bool v);

        // Title / visibility
        void setTitle(std::string_view title);
        void show();
        void hide();

        // Size & DPI
        ExtentU32 windowSize() const;        // logical (points)
        ExtentU32 framebufferSize() const;   // pixels (for swapchain)
        bool      isMinimized() const;       // FB size is 0x0
        bool      wasResized() const;
        void      clearResizedFlag();
        void      contentScale(float& x, float& y) const;

        // Convenience
        void      centerOnPrimaryMonitor();

        // Vulkan WSI helpers
        static VulkanWsiBundle14 wsiBundleForVulkan14();  // NEW: required + recommended ext names
        static std::vector<const char*> requiredVulkanInstanceExtensions(); // legacy helper
        VkSurfaceKHR createVulkanSurface(VkInstance instance) const;

        // Timing
        double    deltaSeconds() const { return dt_; }
        double    smoothedDeltaSeconds() const { return dtSmoothed_; }
        double    elapsedSeconds() const { return elapsed_; }
        uint64_t  frameIndex() const { return frameIndex_; }
        void      setMaxDeltaClamp(double seconds) { dtClamp_ = seconds; }
        void      setDtSmoothingAlpha(float a) { dtAlpha_ = (a < 0.f ? 0.f : (a > 1.f ? 1.f : a)); }

        // Native
        GLFWwindow* glfwHandle() const { return window_; }
#if defined(_WIN32)
        HWND        win32Handle() const { return window_ ? glfwGetWin32Window(window_) : nullptr; }
#endif

        // Callbacks
        void setCallbacks(Callbacks cbs) { callbacks_ = std::move(cbs); }

    private:
        // GLFW global init/terminate guard
        struct GlfwInitGuard {
            GlfwInitGuard();
            ~GlfwInitGuard();
            GlfwInitGuard(const GlfwInitGuard&) = delete;
            GlfwInitGuard& operator=(const GlfwInitGuard&) = delete;
        };

#if defined(_WIN32)
        struct WinPerfGuard {
            explicit WinPerfGuard(const WindowCreateInfo& ci);
            ~WinPerfGuard();
            bool timer1ms = false;
            bool power = false;
            bool prio = false;
            bool nosleep = false;
        };
#endif

    private:
        // Static callbacks
        static void s_errorCallback(int code, const char* desc);
        static void s_framebufferSizeCallback(GLFWwindow* win, int w, int h);
        static void s_windowFocusCallback(GLFWwindow* win, int focused);
        static void s_windowIconifyCallback(GLFWwindow* win, int iconified);
        static void s_windowContentScaleCallback(GLFWwindow* win, float xscale, float yscale);

        // Helpers
        void installCallbacks();
        void updateCachedSizes();
        void createGLFWWindow(const WindowCreateInfo& ci);
        void applyBorderlessToPrimaryMonitor();

        // Delta time
        void tickNow();

    private:
        GlfwInitGuard      glfwGuard_;
#if defined(_WIN32)
        WinPerfGuard       winPerf_;
#endif
        GLFWwindow* window_ = nullptr;

        // Cached state
        mutable ExtentU32  cachedWindowSize_{};
        mutable ExtentU32  cachedFramebufferSize_{};
        mutable float      contentScaleX_ = 1.0f;
        mutable float      contentScaleY_ = 1.0f;
        bool               framebufferResized_ = false;
        Callbacks          callbacks_{};
        WindowMode         modeRequested_ = WindowMode::Windowed;

        // Timing
        using Clock = std::chrono::steady_clock;
        Clock::time_point  start_{};
        Clock::time_point  prev_{};
        double             dt_ = 0.0;   // seconds (clamped)
        double             dtSmoothed_ = 0.0;   // EMA if enabled
        double             elapsed_ = 0.0;   // seconds since start
        uint64_t           frameIndex_ = 0;
        double             dtClamp_ = 0.25;  // seconds
        float              dtAlpha_ = 0.0f;  // EMA alpha [0..1]
    };

} // namespace hvk

#endif // HVK_WINDOW