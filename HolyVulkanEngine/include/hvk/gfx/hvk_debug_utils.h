#ifndef HVK_DEBUG_UTILS_H
#define HVK_DEBUG_UTILS_H

#include <vulkan/vulkan.h>
#include <string_view>

namespace hvk {

    class Device; // fwd

    // Thin helper around VK_EXT_debug_utils.
    // All methods are NO-OPs if the extension is unavailable.
    class DebugUtils {
    public:
        DebugUtils() = default;
        explicit DebugUtils(const Device* device); // safe if debug utils not enabled
        bool supported() const { return has_; }

        // Command buffer labels
        void cmdBeginLabel(VkCommandBuffer cmd, std::string_view name, const float rgba[4]) const;
        void cmdEndLabel(VkCommandBuffer cmd) const;

        // Queue labels (optional, e.g., around submissions)
        void queueBeginLabel(VkQueue q, std::string_view name, const float rgba[4]) const;
        void queueEndLabel(VkQueue q) const;

        // RAII scopes -------------------------------------------------------------

        struct CmdLabelScope {
            CmdLabelScope(const DebugUtils* du, VkCommandBuffer cmd,
                std::string_view name, const float rgba[4]) : du(du), cmd(cmd) {
                if (du) du->cmdBeginLabel(cmd, name, rgba);
            }
            ~CmdLabelScope() { if (du) du->cmdEndLabel(cmd); }
            const DebugUtils* du{};
            VkCommandBuffer   cmd{ VK_NULL_HANDLE };
        };

        struct QueueLabelScope {
            QueueLabelScope(const DebugUtils* du, VkQueue q,
                std::string_view name, const float rgba[4]) : du(du), q(q) {
                if (du) du->queueBeginLabel(q, name, rgba);
            }
            ~QueueLabelScope() { if (du) du->queueEndLabel(q); }
            const DebugUtils* du{};
            VkQueue           q{ VK_NULL_HANDLE };
        };

    private:
        const Device* device_ = nullptr;
        bool has_ = false;

        // Function pointers (device-level)
        PFN_vkCmdBeginDebugUtilsLabelEXT  pCmdBegin_ = nullptr;
        PFN_vkCmdEndDebugUtilsLabelEXT    pCmdEnd_ = nullptr;
        PFN_vkQueueBeginDebugUtilsLabelEXT pQueueBegin_ = nullptr;
        PFN_vkQueueEndDebugUtilsLabelEXT   pQueueEnd_ = nullptr;
    };

} // namespace hvk

#endif // HVK_DEBUG_UTILS_H
