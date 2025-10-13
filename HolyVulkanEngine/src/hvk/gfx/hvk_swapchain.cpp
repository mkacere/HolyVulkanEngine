#include "pch.h"

#include <hvk/gfx/hvk_swapchain.h>

namespace hvk {

#ifndef VK_CHECK
#define VK_CHECK(x) do { VkResult _e = (x); if (_e != VK_SUCCESS) throw std::runtime_error("Vulkan error: " #x); } while(0)
#endif

    // --------------------- helpers ---------------------

    static inline bool contains(const std::vector<VkPresentModeKHR>& v, VkPresentModeKHR m) {
        return std::find(v.begin(), v.end(), m) != v.end();
    }

    static inline bool contains_format(const std::vector<VkSurfaceFormatKHR>& v, VkFormat f, VkColorSpaceKHR cs) {
        for (auto& sf : v) if (sf.format == f && sf.colorSpace == cs) return true;
        return false;
    }

    // --------------------- ctor/dtor/move ---------------------

    Swapchain::Swapchain(const SwapchainCreateInfo& ci)
        : device_(ci.device)
        , surface_(ci.surface ? ci.surface : (ci.device ? ci.device->surface() : VK_NULL_HANDLE))
        , debugBase_(ci.debugBaseName.empty() ? "swap" : std::string(ci.debugBaseName))
        , preferMailbox_(ci.preferMailbox)
        , desiredImageCount_(ci.desiredImageCount ? ci.desiredImageCount : 3)
        , preferredFormats_(ci.preferredFormats)
    {
        if (!device_ || !surface_) throw std::invalid_argument("Swapchain: device/surface is null");

        // default preferred formats if not provided
        if (preferredFormats_.empty()) {
            preferredFormats_ = { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_A2B10G10R10_UNORM_PACK32 };
        }

        usage_ = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
            | ci.extraUsage;

        // Initial extent: if unspecified, try window FB size if you have one; else 0=auto
        VkExtent2D want = ci.desiredExtent;

        create_or_recreate(want);
    }

    Swapchain::~Swapchain() { destroy(); }

    Swapchain::Swapchain(Swapchain&& o) noexcept {
        *this = std::move(o);
    }

    Swapchain& Swapchain::operator=(Swapchain&& o) noexcept {
        if (this != &o) {
            destroy();

            device_ = o.device_;          o.device_ = nullptr;
            surface_ = o.surface_;        o.surface_ = VK_NULL_HANDLE;
            debugBase_ = std::move(o.debugBase_);
            swapchain_ = o.swapchain_;    o.swapchain_ = VK_NULL_HANDLE;
            colorFormat_ = o.colorFormat_;
            colorSpace_ = o.colorSpace_;
            presentMode_ = o.presentMode_;
            extent_ = o.extent_;
            usage_ = o.usage_;
            images_ = std::move(o.images_);
            views_ = std::move(o.views_);
            preferMailbox_ = o.preferMailbox_;
            desiredImageCount_ = o.desiredImageCount_;
            preferredFormats_ = std::move(o.preferredFormats_);
        }
        return *this;
    }

    // --------------------- public API ---------------------

    bool Swapchain::recreateForWindow(const Window& window) {
        // If minimized, delay recreation
        if (window.isMinimized()) return false;
        auto fb = window.framebufferSize();
        if (fb.width == 0 || fb.height == 0) return false;
        return recreate(VkExtent2D{ fb.width, fb.height });
    }

    bool Swapchain::recreate(VkExtent2D newExtent) {
        if (newExtent.width == 0 || newExtent.height == 0) return false;
        return create_or_recreate(newExtent);
    }

    VkResult Swapchain::acquireNextImage(uint32_t& imageIndex, VkSemaphore imageAvailable, uint64_t timeout) {
        // Note: if OUT_OF_DATE_KHR/SUBOPTIMAL_KHR, return it so caller can recreate.
        return vkAcquireNextImageKHR(device_->device(), swapchain_, timeout, imageAvailable, VK_NULL_HANDLE, &imageIndex);
    }

    VkResult Swapchain::present(uint32_t imageIndex, VkQueue presentQueue, VkSemaphore renderFinished) {
        VkSwapchainKHR sc = swapchain_;
        VkPresentInfoKHR pi{ VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
        pi.waitSemaphoreCount = 1;
        pi.pWaitSemaphores = &renderFinished;
        pi.swapchainCount = 1;
        pi.pSwapchains = &sc;
        pi.pImageIndices = &imageIndex;
        // If VK_KHR_present_id is enabled, you could chain VkPresentIdKHR here (omitted, optional).

        return vkQueuePresentKHR(presentQueue, &pi);
    }

    // --------------------- internals ---------------------

    void Swapchain::destroy_views() {
        for (auto v : views_) {
            vkDestroyImageView(device_->device(), v, nullptr);
        }
        views_.clear();
    }

    void Swapchain::create_views() {
        views_.resize(images_.size());
        for (size_t i = 0; i < images_.size(); ++i) {
            VkImageViewCreateInfo vi{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
            vi.image = images_[i];
            vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vi.format = colorFormat_;
            vi.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                              VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
            vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            vi.subresourceRange.baseMipLevel = 0;
            vi.subresourceRange.levelCount = 1;
            vi.subresourceRange.baseArrayLayer = 0;
            vi.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(device_->device(), &vi, nullptr, &views_[i]));

            if (!debugBase_.empty()) {
                device_->setObjectName(VK_OBJECT_TYPE_IMAGE_VIEW,
                    reinterpret_cast<uint64_t>(views_[i]),
                    debugBase_ + "/view#" + std::to_string(i));
            }
        }
    }

    void Swapchain::destroy() {
        if (!device_) return;

        destroy_views();

        if (swapchain_) {
            vkDestroySwapchainKHR(device_->device(), swapchain_, nullptr);
            swapchain_ = VK_NULL_HANDLE;
        }

        images_.clear();
        device_ = nullptr;
        surface_ = VK_NULL_HANDLE;
    }

    VkSurfaceFormatKHR Swapchain::choose_surface_format(const std::vector<VkSurfaceFormatKHR>& avail,
        const std::vector<VkFormat>& prefs) const
    {
        // Prefer SRGB nonlinear in a friendly format from prefs
        for (VkFormat f : prefs) {
            if (contains_format(avail, f, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR))
                return { f, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
        }
        // Fall back to any SRGB nonlinear
        for (const auto& sf : avail) {
            if (sf.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return sf;
        }
        // Fallback: first available
        return avail.front();
    }

    VkPresentModeKHR Swapchain::choose_present_mode(const std::vector<VkPresentModeKHR>& avail, bool preferMailbox) const {
        if (preferMailbox && contains(avail, VK_PRESENT_MODE_MAILBOX_KHR))
            return VK_PRESENT_MODE_MAILBOX_KHR;
        // VK_PRESENT_MODE_FIFO_KHR is guaranteed
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D Swapchain::choose_extent(const VkSurfaceCapabilitiesKHR& caps, VkExtent2D desired) const {
        if (caps.currentExtent.width != UINT32_MAX) {
            // Surface size dictated by the window system
            return caps.currentExtent;
        }
        VkExtent2D e = desired;
        if (e.width == 0 || e.height == 0) e = { 1u, 1u }; // avoid 0; caller should prevent this
        e.width = std::clamp(e.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        e.height = std::clamp(e.height, caps.minImageExtent.height, caps.maxImageExtent.height);
        return e;
    }

    uint32_t Swapchain::choose_image_count(const VkSurfaceCapabilitiesKHR& caps, uint32_t desired) const {
        uint32_t count = (std::max)(desired, caps.minImageCount);
        if (caps.maxImageCount > 0) count = (std::min)(count, caps.maxImageCount);
        return count;
    }

    VkCompositeAlphaFlagBitsKHR Swapchain::choose_composite_alpha(VkCompositeAlphaFlagsKHR supported) const {
        // Prefer OPAQUE; otherwise pick the first available bit.
        if (supported & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        const VkCompositeAlphaFlagBitsKHR options[] = {
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
            VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
        };
        for (auto o : options) if (supported & o) return o;
        return VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // should be supported per spec, but just in case
    }

    bool Swapchain::create_or_recreate(VkExtent2D targetExtent) {
        // Query surface support --------------------------------
        VkPhysicalDevice pd = device_->physical();

        uint32_t fmtCount = 0, pmCount = 0;
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface_, &fmtCount, nullptr));
        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(pd, surface_, &fmtCount, formats.data()));

        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface_, &pmCount, nullptr));
        std::vector<VkPresentModeKHR> presentModes(pmCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(pd, surface_, &pmCount, presentModes.data()));

        VkSurfaceCapabilitiesKHR caps{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(pd, surface_, &caps));

        // Decide choices ---------------------------------------
        const VkSurfaceFormatKHR chosenSF = choose_surface_format(formats, preferredFormats_);
        const VkPresentModeKHR   chosenPM = choose_present_mode(presentModes, preferMailbox_);
        const VkExtent2D         chosenExtent = choose_extent(caps, targetExtent);
        const uint32_t           chosenImages = choose_image_count(caps, desiredImageCount_);
        const VkCompositeAlphaFlagBitsKHR comp = choose_composite_alpha(caps.supportedCompositeAlpha);

        // Create swapchain -------------------------------------
        // If graphics and present queues are different families, we must use CONCURRENT sharing
        const uint32_t gfxFamily = device_->graphics().family;
        const uint32_t presFamily = device_->present().family;
        const bool concurrent = (gfxFamily != presFamily);
        const uint32_t qFamilies[2] = { gfxFamily, presFamily };

        VkSwapchainKHR old = swapchain_;

        VkSwapchainCreateInfoKHR ci{ VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
        ci.surface = surface_;
        ci.minImageCount = chosenImages;
        ci.imageFormat = chosenSF.format;
        ci.imageColorSpace = chosenSF.colorSpace;
        ci.imageExtent = chosenExtent;
        ci.imageArrayLayers = 1;
        ci.imageUsage = usage_;
        ci.imageSharingMode = concurrent ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
        ci.queueFamilyIndexCount = concurrent ? 2u : 0u;
        ci.pQueueFamilyIndices = concurrent ? qFamilies : nullptr;
        ci.preTransform = caps.currentTransform;
        ci.compositeAlpha = comp;
        ci.presentMode = chosenPM;
        ci.clipped = VK_TRUE;
        ci.oldSwapchain = old;

        // Create or recreate
        VkSwapchainKHR newSwap = VK_NULL_HANDLE;
        VK_CHECK(vkCreateSwapchainKHR(device_->device(), &ci, nullptr, &newSwap));

        // Destroy old views & swapchain
        destroy_views();
        if (old) vkDestroySwapchainKHR(device_->device(), old, nullptr);

        swapchain_ = newSwap;
        colorFormat_ = chosenSF.format;
        colorSpace_ = chosenSF.colorSpace;
        presentMode_ = chosenPM;
        extent_ = chosenExtent;

        if (!debugBase_.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_SWAPCHAIN_KHR,
                reinterpret_cast<uint64_t>(swapchain_), debugBase_ + "/swapchain");
        }

        // Fetch images
        uint32_t imgCount = 0;
        VK_CHECK(vkGetSwapchainImagesKHR(device_->device(), swapchain_, &imgCount, nullptr));
        images_.resize(imgCount);
        VK_CHECK(vkGetSwapchainImagesKHR(device_->device(), swapchain_, &imgCount, images_.data()));

        // Name images
        if (!debugBase_.empty()) {
            for (uint32_t i = 0; i < imgCount; ++i) {
                device_->setObjectName(VK_OBJECT_TYPE_IMAGE,
                    reinterpret_cast<uint64_t>(images_[i]),
                    debugBase_ + "/image#" + std::to_string(i));
            }
        }

        // Create views
        create_views();

        return true;
    }

} // namespace hvk
