#ifndef HVK_CMD_LIST_HPP
#define HVK_CMD_LIST_HPP

#include <vulkan/vulkan.h>
#include <vector>
#include <initializer_list>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <type_traits>

#ifndef HVK_VK_CHECK
#define HVK_VK_CHECK(x) do { VkResult _hvk_res = (x); if (_hvk_res != VK_SUCCESS) throw std::runtime_error("Vulkan error: " #x); } while(0)
#endif

namespace hvk {

    // ================== attachment pod structs (namespace-level) ===================
    // kept for backward compatibility; CmdList exposes these as nested aliases too.

    struct ColorAttachment {
        VkImageView            view = VK_NULL_HANDLE;
        VkImageLayout          layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
        VkAttachmentLoadOp     loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentStoreOp    storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        VkClearColorValue      clear = { {0.f, 0.f, 0.f, 1.f} };

        // Optional MSAA resolve
        VkResolveModeFlagBits  resolveMode = VK_RESOLVE_MODE_NONE;
        VkImageView            resolveView = VK_NULL_HANDLE;
        VkImageLayout          resolveLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    };

    struct DepthAttachment {
        VkImageView              view = VK_NULL_HANDLE;
        VkImageLayout            layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        VkAttachmentLoadOp       loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        VkAttachmentStoreOp      storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkClearDepthStencilValue clear = { 1.0f, 0 };

        // Optional resolve
        VkResolveModeFlagBits  resolveMode = VK_RESOLVE_MODE_NONE;
        VkImageView            resolveView = VK_NULL_HANDLE;
        VkImageLayout          resolveLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    };

    // =============================== CmdList wrapper ===============================

    class CmdList {
    public:
        CmdList() = default;
        explicit CmdList(VkCommandBuffer cmd) : cmd_(cmd) {}
        void reset(VkCommandBuffer cmd) { cmd_ = cmd; }

        VkCommandBuffer handle() const { return cmd_; }
        explicit operator bool() const { return cmd_ != VK_NULL_HANDLE; }

        // expose attachment types as nested aliases so RG code can use CmdList::*
        using ColorAttachment = ::hvk::ColorAttachment;
        using DepthAttachment = ::hvk::DepthAttachment;

        // -------------------------------- begin/end --------------------------------
        void begin(VkCommandBufferUsageFlags flags = 0) const {
            VkCommandBufferBeginInfo bi{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            bi.flags = flags;
            HVK_VK_CHECK(vkBeginCommandBuffer(cmd_, &bi));
        }
        void beginOneTime() const { begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); }
        void end() const { HVK_VK_CHECK(vkEndCommandBuffer(cmd_)); }

        // ----------------------------- dynamic rendering ----------------------------
        void beginRendering(const VkRect2D& renderArea,
            const std::vector<ColorAttachment>& colorAttachments,
            const DepthAttachment* depth = nullptr,
            VkRenderingFlags flags = 0) const
        {
            std::vector<VkRenderingAttachmentInfo> cols;
            cols.reserve(colorAttachments.size());
            for (const auto& c : colorAttachments) {
                VkRenderingAttachmentInfo ai{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
                ai.imageView = c.view;
                ai.imageLayout = c.layout;
                ai.resolveMode = c.resolveMode;
                ai.resolveImageView = c.resolveView;
                ai.resolveImageLayout = c.resolveLayout;
                ai.loadOp = c.loadOp;
                ai.storeOp = c.storeOp;
                ai.clearValue.color = c.clear;
                cols.push_back(ai);
            }

            VkRenderingAttachmentInfo depthAI{ VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO };
            const VkRenderingAttachmentInfo* pDepth = nullptr;
            if (depth && depth->view != VK_NULL_HANDLE) {
                depthAI.imageView = depth->view;
                depthAI.imageLayout = depth->layout;
                depthAI.resolveMode = depth->resolveMode;
                depthAI.resolveImageView = depth->resolveView;
                depthAI.resolveImageLayout = depth->resolveLayout;
                depthAI.loadOp = depth->loadOp;
                depthAI.storeOp = depth->storeOp;
                depthAI.clearValue.depthStencil = depth->clear;
                pDepth = &depthAI;
            }

            VkRenderingInfo ri{ VK_STRUCTURE_TYPE_RENDERING_INFO };
            ri.flags = flags;
            ri.renderArea = renderArea;
            ri.layerCount = 1;
            ri.viewMask = 0;
            ri.colorAttachmentCount = static_cast<uint32_t>(cols.size());
            ri.pColorAttachments = cols.empty() ? nullptr : cols.data();
            ri.pDepthAttachment = pDepth;
            ri.pStencilAttachment = nullptr;

            vkCmdBeginRendering(cmd_, &ri);
        }

        // single-color helper
        void beginRenderingColor(VkImageView colorView, VkExtent2D extent,
            VkClearColorValue clear = { {0,0,0,1} },
            VkAttachmentLoadOp loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            VkAttachmentStoreOp storeOp = VK_ATTACHMENT_STORE_OP_STORE) const
        {
            ColorAttachment ca{};
            ca.view = colorView;
            ca.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
            ca.loadOp = loadOp;
            ca.storeOp = storeOp;
            ca.clear = clear;

            VkRect2D area{ {0,0}, {extent.width, extent.height} };
            beginRendering(area, std::vector<ColorAttachment>{ca}, nullptr, 0);
        }

        void endRendering() const { vkCmdEndRendering(cmd_); }

        // ------------------------------- dynamic state ------------------------------
        void setViewport(float x, float y, float width, float height,
            float minDepth = 0.f, float maxDepth = 1.f) const {
            VkViewport vp{ x, y, width, height, minDepth, maxDepth };
            vkCmdSetViewport(cmd_, 0, 1, &vp);
        }
        void setViewport(VkExtent2D extent, bool yDown = false) const {
            if (yDown) {
                VkViewport vp{ 0.f, static_cast<float>(extent.height),
                               static_cast<float>(extent.width), -static_cast<float>(extent.height),
                               0.f, 1.f };
                vkCmdSetViewport(cmd_, 0, 1, &vp);
            }
            else {
                VkViewport vp{ 0.f, 0.f,
                               static_cast<float>(extent.width), static_cast<float>(extent.height),
                               0.f, 1.f };
                vkCmdSetViewport(cmd_, 0, 1, &vp);
            }
        }
        void setScissor(int32_t x, int32_t y, uint32_t width, uint32_t height) const {
            VkRect2D sc{ {x,y},{width,height} };
            vkCmdSetScissor(cmd_, 0, 1, &sc);
        }
        void setViewportScissor(VkExtent2D extent, bool yDown = false) const {
            setViewport(extent, yDown);
            setScissor(0, 0, extent.width, extent.height);
        }

        // extended dynamic state (only if your pipeline enables them)
        void setCullMode(VkCullModeFlags mode) const { vkCmdSetCullMode(cmd_, mode); }
        void setFrontFace(VkFrontFace ff) const { vkCmdSetFrontFace(cmd_, ff); }
        void setPrimitiveTopology(VkPrimitiveTopology t) const { vkCmdSetPrimitiveTopology(cmd_, t); }
        void setDepthTestEnable(VkBool32 e) const { vkCmdSetDepthTestEnable(cmd_, e); }
        void setDepthWriteEnable(VkBool32 e) const { vkCmdSetDepthWriteEnable(cmd_, e); }
        void setStencilTestEnable(VkBool32 e) const { vkCmdSetStencilTestEnable(cmd_, e); }
        void setBlendConstants(const float bc[4]) const { vkCmdSetBlendConstants(cmd_, bc); }
        void setLineWidth(float w) const { vkCmdSetLineWidth(cmd_, w); }
        void setDepthBias(float constantFactor, float clamp, float slopeFactor) const {
            vkCmdSetDepthBias(cmd_, constantFactor, clamp, slopeFactor);
        }

        // -------------------------------- bindings ----------------------------------
        void bindGraphicsPipeline(VkPipeline pipeline) const {
            vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        }
        void bindComputePipeline(VkPipeline pipeline) const {
            vkCmdBindPipeline(cmd_, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        }

        void bindDescriptorSets(VkPipelineBindPoint bindPoint,
            VkPipelineLayout layout,
            uint32_t firstSet,
            uint32_t setCount,
            const VkDescriptorSet* sets,
            uint32_t dynamicOffsetCount = 0,
            const uint32_t* dynamicOffsets = nullptr) const
        {
            vkCmdBindDescriptorSets(cmd_, bindPoint, layout,
                firstSet, setCount, sets,
                dynamicOffsetCount, dynamicOffsets);
        }

        // graphics convenience
        void bindGraphicsDescriptorSets(VkPipelineLayout layout,
            uint32_t firstSet,
            uint32_t setCount,
            const VkDescriptorSet* sets,
            uint32_t dynamicOffsetCount = 0,
            const uint32_t* dynamicOffsets = nullptr) const
        {
            bindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, firstSet, setCount, sets,
                dynamicOffsetCount, dynamicOffsets);
        }
        void bindGraphicsDescriptorSets(VkPipelineLayout layout,
            uint32_t firstSet,
            std::initializer_list<VkDescriptorSet> sets,
            std::initializer_list<uint32_t> dynOffsets = {}) const
        {
            const auto* s = sets.begin();
            uint32_t sc = static_cast<uint32_t>(sets.size());
            const auto* d = dynOffsets.begin();
            uint32_t dc = static_cast<uint32_t>(dynOffsets.size());
            bindGraphicsDescriptorSets(layout, firstSet, sc, s, dc, d);
        }

        // compute convenience
        void bindComputeDescriptorSets(VkPipelineLayout layout,
            uint32_t firstSet,
            uint32_t setCount,
            const VkDescriptorSet* sets,
            uint32_t dynamicOffsetCount = 0,
            const uint32_t* dynamicOffsets = nullptr) const
        {
            bindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, layout, firstSet, setCount, sets,
                dynamicOffsetCount, dynamicOffsets);
        }

        // buffers
        void bindVertexBuffers(uint32_t firstBinding,
            uint32_t bindingCount,
            const VkBuffer* buffers,
            const VkDeviceSize* offsets) const
        {
            vkCmdBindVertexBuffers(cmd_, firstBinding, bindingCount, buffers, offsets);
        }
        void bindVertexBuffer(uint32_t binding, VkBuffer buffer, VkDeviceSize offset = 0) const {
            VkBuffer b = buffer; VkDeviceSize off = offset;
            vkCmdBindVertexBuffers(cmd_, binding, 1, &b, &off);
        }
        void bindIndexBuffer(VkBuffer buffer, VkDeviceSize offset = 0, VkIndexType type = VK_INDEX_TYPE_UINT32) const {
            vkCmdBindIndexBuffer(cmd_, buffer, offset, type);
        }

        // push constants
        template<typename T>
        void pushConstants(VkPipelineLayout layout, VkShaderStageFlags stages, const T& data, uint32_t offset = 0) const {
            static_assert(std::is_trivially_copyable<T>::value, "pushConstants<T>: T must be trivially copyable");
            static_assert(sizeof(T) % 4 == 0, "pushConstants<T>: size must be multiple of 4 bytes");
            vkCmdPushConstants(cmd_, layout, stages, offset, static_cast<uint32_t>(sizeof(T)), &data);
        }

        // -------------------------------- draw/dispatch ------------------------------
        void draw(uint32_t vertexCount,
            uint32_t instanceCount = 1,
            uint32_t firstVertex = 0,
            uint32_t firstInstance = 0) const
        {
            vkCmdDraw(cmd_, vertexCount, instanceCount, firstVertex, firstInstance);
        }
        void drawIndexed(uint32_t indexCount,
            uint32_t instanceCount = 1,
            uint32_t firstIndex = 0,
            int32_t  vertexOffset = 0,
            uint32_t firstInstance = 0) const
        {
            vkCmdDrawIndexed(cmd_, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        }
        void drawIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) const {
            vkCmdDrawIndirect(cmd_, buffer, offset, drawCount, stride);
        }
        void drawIndexedIndirect(VkBuffer buffer, VkDeviceSize offset, uint32_t drawCount, uint32_t stride) const {
            vkCmdDrawIndexedIndirect(cmd_, buffer, offset, drawCount, stride);
        }
        void dispatch(uint32_t gx, uint32_t gy, uint32_t gz) const { vkCmdDispatch(cmd_, gx, gy, gz); }
        void dispatchIndirect(VkBuffer buffer, VkDeviceSize offset) const { vkCmdDispatchIndirect(cmd_, buffer, offset); }

        // -------------------------------- transfers ---------------------------------
        void copyBuffer(VkBuffer src, VkDeviceSize srcOffset, VkBuffer dst, VkDeviceSize dstOffset, VkDeviceSize size) const {
            VkBufferCopy r{ srcOffset, dstOffset, size };
            vkCmdCopyBuffer(cmd_, src, dst, 1, &r);
        }
        void fillBuffer(VkBuffer dst, VkDeviceSize dstOffset, VkDeviceSize size, uint32_t data) const {
            vkCmdFillBuffer(cmd_, dst, dstOffset, size, data);
        }
        // note: size <= 64KB and 4-byte aligned
        void updateBuffer(VkBuffer dst, VkDeviceSize dstOffset, VkDeviceSize size, const void* src) const {
            vkCmdUpdateBuffer(cmd_, dst, dstOffset, size, src);
        }
        void copyBufferToImage(VkBuffer src, VkImage dst,
            const VkBufferImageCopy& region,
            VkImageLayout dstLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) const
        {
            vkCmdCopyBufferToImage(cmd_, src, dst, dstLayout, 1, &region);
        }
        void copyImage(VkImage src, VkImageLayout srcLayout,
            VkImage dst, VkImageLayout dstLayout,
            const VkImageCopy& region) const
        {
            vkCmdCopyImage(cmd_, src, srcLayout, dst, dstLayout, 1, &region);
        }
        void blitImage(VkImage src, VkImageLayout srcLayout,
            VkImage dst, VkImageLayout dstLayout,
            const VkImageBlit& region,
            VkFilter filter = VK_FILTER_LINEAR) const
        {
            vkCmdBlitImage(cmd_, src, srcLayout, dst, dstLayout, 1, &region, filter);
        }
        void clearColorImage(VkImage image, VkImageLayout layout,
            const VkClearColorValue& color,
            const VkImageSubresourceRange& range) const
        {
            vkCmdClearColorImage(cmd_, image, layout, &color, 1, &range);
        }
        void clearDepthStencilImage(VkImage image, VkImageLayout layout,
            const VkClearDepthStencilValue& value,
            const VkImageSubresourceRange& range) const
        {
            vkCmdClearDepthStencilImage(cmd_, image, layout, &value, 1, &range);
        }

    private:
        VkCommandBuffer cmd_ = VK_NULL_HANDLE;
    };

} // namespace hvk

#endif // HVK_CMD_LIST_HPP
