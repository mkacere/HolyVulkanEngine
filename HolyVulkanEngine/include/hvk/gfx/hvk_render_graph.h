/**
 * @file hvk_render_graph.h
 * @brief Frame graph for automatic rendering dependency management
 * @author Holy Vulkan Engine
 * @date 2025
 * Manages render passes, resources, and automatic barrier insertion for
 * frame-based rendering with dependency tracking.
 */

#ifndef HVK_RENDER_GRAPH_H
#define HVK_RENDER_GRAPH_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <optional>
#include <stdexcept>

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_cmd_list.hpp>
#include <hvk/gfx/hvk_gpu_resources.h>
#include <hvk/gfx/hvk_barriers.hpp>
#include <hvk/gfx/hvk_debug_utils.h>

namespace hvk {

    class RenderGraph {
    public:
        // ---------------- Handles ----------------
        struct ImageHandle {
            uint32_t id = ~0u;
            bool valid() const { return id != ~0u; }
            explicit operator bool() const { return valid(); }
        };

        struct PassHandle {
            uint32_t id = ~0u;
            bool valid() const { return id != ~0u; }
            explicit operator bool() const { return valid(); }
        };

        // ---------------- Sizes ------------------
        struct Size2D {
            enum class Class { Absolute, SwapchainRelative } cls = Class::Absolute;
            uint32_t width = 0, height = 0;
            uint32_t scaleNum = 1, scaleDen = 1;
            static Size2D Absolute(uint32_t w, uint32_t h) { Size2D s; s.cls = Class::Absolute; s.width = w; s.height = h; return s; }
            static Size2D SwapchainRel(uint32_t num = 1, uint32_t den = 1) { Size2D s; s.cls = Class::SwapchainRelative; s.scaleNum = num; s.scaleDen = den ? den : 1; return s; }
        };

        // --------------- Images ------------------
        struct ImageDesc {
            Size2D      size = Size2D::Absolute(1, 1);
            VkFormat    format = VK_FORMAT_B8G8R8A8_SRGB;
            uint32_t    mipLevels = 1;
            uint32_t    arrayLayers = 1;
            VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;

            bool canColor = true;
            bool canDepth = false;
            bool canSample = true;
            bool canStorage = false;
            bool canTransferSrc = false;
            bool canTransferDst = false;

            std::string debugName{};
        };

        struct ExternalImage {
            VkImage     image = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
            VkFormat    format = VK_FORMAT_B8G8R8A8_SRGB;
            VkExtent2D  extent{ 0,0 };
            hvk::barrier::ImgUse initialUse = hvk::barrier::ImgUse::Present;
            std::string debugName{};
        };

        // --------------- IO Descs ---------------
        struct ColorWrite {
            ImageHandle img;
            VkAttachmentLoadOp  load = VK_ATTACHMENT_LOAD_OP_CLEAR;
            VkAttachmentStoreOp store = VK_ATTACHMENT_STORE_OP_STORE;
            VkClearColorValue   clear = { {0.f,0.f,0.f,1.f} };
            VkImageView         viewOverride = VK_NULL_HANDLE;
        };

        struct DepthWrite {
            ImageHandle img;
            VkAttachmentLoadOp  load = VK_ATTACHMENT_LOAD_OP_CLEAR;
            VkAttachmentStoreOp store = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            VkClearDepthStencilValue clear{ 1.f,0 };
            bool readOnly = false;
            VkImageView viewOverride = VK_NULL_HANDLE;
        };

        struct SampledRead { ImageHandle img; };
        struct StorageRead { ImageHandle img; };
        struct StorageWrite { ImageHandle img; };
        struct Present { ImageHandle img; };

        enum class PassKind { Graphics, Compute /*, Transfer*/ };

        class PassBuilder {
        public:
            void setKind(PassKind k) { kind = k; }
            PassKind getKind() const { return kind; }

            void writeColor(const ColorWrite& c) { colors.push_back(c); }
            void writeDepth(const DepthWrite& d) { depth = d; hasDepth = true; }
            void readSampled(ImageHandle i) { readsSampled.push_back({ i }); }
            void readStorage(ImageHandle i) { readsStorage.push_back({ i }); }
            void writeStorage(ImageHandle i) { writesStorage.push_back({ i }); }
            void present(ImageHandle i) { presents.push_back({ i }); }

        private:
            friend class RenderGraph;
            PassKind kind = PassKind::Graphics;
            std::vector<ColorWrite> colors;
            DepthWrite depth{};
            bool       hasDepth = false;
            std::vector<SampledRead>  readsSampled;
            std::vector<StorageRead>  readsStorage;
            std::vector<StorageWrite> writesStorage;
            std::vector<Present>      presents;
        };

        class PassContext {
        public:
            VkExtent2D extent() const { return extent_; }
            const std::vector<VkImageView>& colorViews() const { return colorViews_; }
            VkImageView depthView() const { return depthView_; }
        private:
            friend class RenderGraph;
            VkExtent2D extent_{ 0,0 };
            std::vector<VkImageView> colorViews_;
            VkImageView depthView_ = VK_NULL_HANDLE;
        };

        // --------------- Create/Frame -----------
        struct CreateInfo {
            const Device* device = nullptr;
            uint32_t      framesInFlight = 3;
            const DebugUtils* debugUtils = nullptr;
            bool autoLabels = true;
        };

        explicit RenderGraph(const CreateInfo& ci);
        ~RenderGraph();

        void beginFrame(uint32_t frameIndex, VkExtent2D swapExtent);
        void clear();

        // --------------- Resources --------------
        ImageHandle createImage(const ImageDesc& d);
        ImageHandle importExternalImage(const ExternalImage& e);

        // --------------- Passes -----------------
        using PassSetupFn = std::function<void(PassBuilder&)>;
        using PassRecordFn = std::function<void(CmdList&, const PassContext&)>;

        PassHandle addPass(std::string_view name, PassSetupFn setup, PassRecordFn record);
        // Sugar:
        PassHandle addGraphicsPass(std::string_view name, PassSetupFn setup, PassRecordFn record);
        PassHandle addComputePass(std::string_view name, PassSetupFn setup, PassRecordFn record);

        // --------------- Build/Run --------------
        void compile();             // build order + precompute barriers
        void execute(CmdList& cmd); // apply barriers, open rendering (if needed), call record

        // --------------- Helpers ----------------
        void markForPresent(ImageHandle img);
        VkImageView imageView(ImageHandle h) const;
        VkFormat    imageFormat(ImageHandle h) const;
        VkExtent2D  imageExtent(ImageHandle h) const;

    private:
        struct Img {
            ImageDesc desc{};
            bool isExternal = false;

            VkImage     image = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
            VkFormat    format = VK_FORMAT_UNDEFINED;
            VkExtent2D  extent{ 0,0 };

            hvk::barrier::ImgUse lastUse = hvk::barrier::ImgUse::Undefined;

            GpuImage  ownedImage;
            ImageView ownedView;
        };

        struct Pass {
            std::string name;
            PassKind kind = PassKind::Graphics;
            PassBuilder io;
            PassRecordFn record;

            VkExtent2D   extent{ 0,0 };
            std::vector<VkImageView> colorViews;
            VkImageView depthView = VK_NULL_HANDLE;

            // Precomputed barriers for this pass
            std::vector<VkImageMemoryBarrier2> preBarriers;
        };

    private:
        // utils
        VkExtent2D resolveSize(const Size2D& s) const;
        void realizeTransientImage(uint32_t idx);
        void destroyCurrentFrameResources();

        void validatePassIO(const Pass& p) const;
        static hvk::barrier::ImgUse useForColorWrite() { return hvk::barrier::ImgUse::ColorAttachment; }
        static hvk::barrier::ImgUse useForDepthWrite(bool ro) {
            return ro ? hvk::barrier::ImgUse::DepthStencilReadOnly
                : hvk::barrier::ImgUse::DepthStencilAttachment;
        }
        static hvk::barrier::ImgUse useForSampledRead() { return hvk::barrier::ImgUse::ShaderRead; }
        static hvk::barrier::ImgUse useForStorageRead() { return hvk::barrier::ImgUse::ShaderRead; }
        static hvk::barrier::ImgUse useForStorageWrite() { return hvk::barrier::ImgUse::ShaderWrite; }

        void buildBarriersForPass(uint32_t passIdx, std::vector<VkImageMemoryBarrier2>& out) const;
        void applyBarriers(CmdList& cmd, const std::vector<VkImageMemoryBarrier2>& imgBarriers) const;

    private:
        const Device* dev_ = nullptr;
        const DebugUtils* dbg_ = nullptr;
        bool autoLabels_ = true;

        uint32_t frames_ = 0;
        uint32_t curFrame_ = 0;
        VkExtent2D swapExtent_{ 0,0 };

        std::vector<Img>  images_;
        std::vector<Pass> passes_;
        std::vector<bool> finalPresent_;
    };

} // namespace hvk

#endif // HVK_RENDER_GRAPH_H
