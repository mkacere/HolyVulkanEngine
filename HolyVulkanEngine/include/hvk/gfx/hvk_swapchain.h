#ifndef HVK_SWAPCHAIN_H
#define HVK_SWAPCHAIN_H

#include <vulkan/vulkan.h>
#include <vector>
#include <string_view>
#include <cstdint>
#include <optional>

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_window.h>

namespace hvk {

    // Creation knobs (safe defaults)
    struct SwapchainCreateInfo {
        const Device* device = nullptr;            // required
        VkSurfaceKHR  surface = VK_NULL_HANDLE;    // usually device->surface()
        VkExtent2D    desiredExtent{ 0,0 };          // if (0,0) => use window FB size
        bool          preferMailbox = true;        // else FIFO
        uint32_t      desiredImageCount = 3;       // try triple-buffer
        // Extra usage for images (base always includes COLOR_ATTACHMENT|TRANSFER_SRC|TRANSFER_DST)
        VkImageUsageFlags extraUsage = 0;          // e.g. VK_IMAGE_USAGE_SAMPLED_BIT if you sample swapchain
        // Preferred color formats (first match wins). Leave empty to use sensible defaults.
        std::vector<VkFormat> preferredFormats;    // e.g. {B8G8R8A8_SRGB, R8G8R8A8_SRGB}
        std::string_view debugBaseName{};          // "swap"
    };

    class Swapchain {
    public:
        Swapchain() = default;
        explicit Swapchain(const SwapchainCreateInfo& ci);
        ~Swapchain();

        Swapchain(const Swapchain&) = delete;
        Swapchain& operator=(const Swapchain&) = delete;

        Swapchain(Swapchain&&) noexcept;
        Swapchain& operator=(Swapchain&&) noexcept;

        // Recreate if window size changed or after OUT_OF_DATE/SUBOPTIMAL.
        // If window is minimized (0x0), returns false and keeps old swapchain untouched.
        bool recreateForWindow(const Window& window);

        // Explicit recreate with extent (width/height). Returns false if extent==0.
        bool recreate(VkExtent2D newExtent);

        // Acquire/present (use your FrameSync semaphores)
        VkResult acquireNextImage(uint32_t& imageIndex, VkSemaphore imageAvailable, uint64_t timeout = UINT64_MAX);
        VkResult present(uint32_t imageIndex, VkQueue presentQueue, VkSemaphore renderFinished);

        // Getters
        VkSwapchainKHR handle() const { return swapchain_; }
        VkFormat       colorFormat() const { return colorFormat_; }
        VkColorSpaceKHR colorSpace() const { return colorSpace_; }
        VkPresentModeKHR presentMode() const { return presentMode_; }
        VkExtent2D     extent() const { return extent_; }
        uint32_t       imageCount() const { return static_cast<uint32_t>(images_.size()); }
        VkImage        image(uint32_t i) const { return images_[i]; }
        VkImageView    imageView(uint32_t i) const { return views_[i]; }

        // Convenience: all views
        const std::vector<VkImageView>& imageViews() const { return views_; }

    private:
        void destroy();
        bool create_or_recreate(VkExtent2D targetExtent);
        void destroy_views();
        void create_views();

        // choices
        VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR>& avail,
            const std::vector<VkFormat>& prefs) const;
        VkPresentModeKHR   choose_present_mode(const std::vector<VkPresentModeKHR>& avail, bool preferMailbox) const;
        VkExtent2D         choose_extent(const VkSurfaceCapabilitiesKHR& caps, VkExtent2D desired) const;
        uint32_t           choose_image_count(const VkSurfaceCapabilitiesKHR& caps, uint32_t desired) const;
        VkCompositeAlphaFlagBitsKHR choose_composite_alpha(VkCompositeAlphaFlagsKHR supported) const;

    private:
        // immutable-ish after (re)create
        const Device* device_ = nullptr;
        VkSurfaceKHR  surface_ = VK_NULL_HANDLE;
        std::string   debugBase_;

        // swapchain state
        VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
        VkFormat       colorFormat_ = VK_FORMAT_B8G8R8A8_SRGB;
        VkColorSpaceKHR colorSpace_ = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        VkPresentModeKHR presentMode_ = VK_PRESENT_MODE_FIFO_KHR;
        VkExtent2D     extent_{ 0,0 };
        VkImageUsageFlags usage_ = 0;

        // images + views
        std::vector<VkImage>     images_;
        std::vector<VkImageView> views_;

        // config knobs
        bool      preferMailbox_ = true;
        uint32_t  desiredImageCount_ = 3;
        std::vector<VkFormat> preferredFormats_;
    };

} // namespace hvk

#endif // HVK_SWAPCHAIN_H
