#include "pch.h"

#include <hvk/gfx/hvk_debug_utils.h>
#include <hvk/gfx/hvk_device.h>

namespace hvk {

    DebugUtils::DebugUtils(const Device* device)
        : device_(device)
    {
        if (!device_) return;

        // If VK_EXT_debug_utils was enabled at instance/device creation, these will be valid.
        pCmdBegin_ = reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device_->device(), "vkCmdBeginDebugUtilsLabelEXT"));
        pCmdEnd_ = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device_->device(), "vkCmdEndDebugUtilsLabelEXT"));
        pQueueBegin_ = reinterpret_cast<PFN_vkQueueBeginDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device_->device(), "vkQueueBeginDebugUtilsLabelEXT"));
        pQueueEnd_ = reinterpret_cast<PFN_vkQueueEndDebugUtilsLabelEXT>(
            vkGetDeviceProcAddr(device_->device(), "vkQueueEndDebugUtilsLabelEXT"));

        has_ = (pCmdBegin_ && pCmdEnd_); // cmd labels are what we primarily need
    }

    void DebugUtils::cmdBeginLabel(VkCommandBuffer cmd, std::string_view name, const float rgba[4]) const {
        if (!has_) return;
        VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName = name.data();
        if (rgba) std::memcpy(label.color, rgba, 4 * sizeof(float));
        pCmdBegin_(cmd, &label);
    }

    void DebugUtils::cmdEndLabel(VkCommandBuffer cmd) const {
        if (!has_) return;
        pCmdEnd_(cmd);
    }

    void DebugUtils::queueBeginLabel(VkQueue q, std::string_view name, const float rgba[4]) const {
        if (!has_ || !pQueueBegin_) return;
        VkDebugUtilsLabelEXT label{ VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT };
        label.pLabelName = name.data();
        if (rgba) std::memcpy(label.color, rgba, 4 * sizeof(float));
        pQueueBegin_(q, &label);
    }

    void DebugUtils::queueEndLabel(VkQueue q) const {
        if (!has_ || !pQueueEnd_) return;
        pQueueEnd_(q);
    }

} // namespace hvk
