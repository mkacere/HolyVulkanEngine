#include "pch.h"
#include <hvk/gfx/hvk_render_graph.h>

namespace hvk {

    static inline VkImageAspectFlags aspect_from_format(VkFormat f) {
        return hvk::barrier::aspect_from_format(f);
    }

    RenderGraph::RenderGraph(const CreateInfo& ci)
        : dev_(ci.device), dbg_(ci.debugUtils), autoLabels_(ci.autoLabels),
        frames_(ci.framesInFlight ? ci.framesInFlight : 2) {
        if (!dev_) throw std::invalid_argument("RenderGraph: device is null");
    }

    RenderGraph::~RenderGraph() { /* per-frame only; nothing persistent */ }

    void RenderGraph::clear() {
        destroyCurrentFrameResources();
        images_.clear();
        passes_.clear();
        finalPresent_.clear();
    }

    void RenderGraph::beginFrame(uint32_t frameIndex, VkExtent2D swapExtent) {
        curFrame_ = frameIndex % frames_;
        swapExtent_ = swapExtent;
        clear();
    }

    // ---------------- Resources ----------------

    RenderGraph::ImageHandle RenderGraph::createImage(const ImageDesc& d) {
        // Allocate slot first (fixes "realize before push" bug)
        ImageHandle h{ static_cast<uint32_t>(images_.size()) };
        Img img{};
        img.desc = d;
        img.isExternal = false;
        img.format = d.format;
        img.extent = resolveSize(d.size);
        img.lastUse = hvk::barrier::ImgUse::Undefined;

        images_.push_back(std::move(img));
        finalPresent_.push_back(false);

        // Realize after the slot exists
        realizeTransientImage(h.id);
        return h;
    }

    RenderGraph::ImageHandle RenderGraph::importExternalImage(const ExternalImage& e) {
        if (!e.image || !e.view) throw std::invalid_argument("importExternalImage: handles are null");
        ImageHandle h{ static_cast<uint32_t>(images_.size()) };
        Img img{};
        img.isExternal = true;
        img.image = e.image;
        img.view = e.view;
        img.format = e.format;
        img.extent = e.extent;
        img.lastUse = e.initialUse;

        images_.push_back(std::move(img));
        finalPresent_.push_back(false);
        return h;
    }

    // --------------- Passes --------------------

    RenderGraph::PassHandle RenderGraph::addPass(std::string_view name, PassSetupFn setup, PassRecordFn record) {
        if (!record) throw std::invalid_argument("addPass: record callback is null");
        Pass p{};
        p.name = std::string(name);
        setup(p.io);

        // Resolve pass kind default: if any color/depth attachment, consider Graphics
        if (p.io.colors.empty() && !p.io.hasDepth) {
            // If user didn’t explicitly set kind->Compute, default to Graphics-less compute.
            // We keep whatever builder set (default Graphics) but compute is usually intended with no attachments.
            // Caller can force via builder.setKind(...).
        }

        // Resolve extent from first color or depth, else swap
        if (!p.io.colors.empty())      p.extent = imageExtent(p.io.colors[0].img);
        else if (p.io.hasDepth)        p.extent = imageExtent(p.io.depth.img);
        else                           p.extent = swapExtent_;

        // Cache views
        for (auto& c : p.io.colors) {
            p.colorViews.push_back(c.viewOverride ? c.viewOverride : imageView(c.img));
        }
        if (p.io.hasDepth) {
            p.depthView = p.io.depth.viewOverride ? p.io.depth.viewOverride : imageView(p.io.depth.img);
        }

        p.kind = p.io.getKind();
        p.record = std::move(record);

        // Basic same-pass validation
        validatePassIO(p);

        PassHandle ph{ static_cast<uint32_t>(passes_.size()) };
        passes_.push_back(std::move(p));
        return ph;
    }

    RenderGraph::PassHandle RenderGraph::addGraphicsPass(std::string_view name, PassSetupFn setup, PassRecordFn record) {
        return addPass(name,
            [setup](PassBuilder& b) { b.setKind(PassKind::Graphics); setup(b); },
            std::move(record));
    }

    RenderGraph::PassHandle RenderGraph::addComputePass(std::string_view name, PassSetupFn setup, PassRecordFn record) {
        return addPass(name,
            [setup](PassBuilder& b) { b.setKind(PassKind::Compute); setup(b); },
            std::move(record));
    }

    void RenderGraph::validatePassIO(const Pass& p) const {
        // Simple rule: don’t read & write the same image in one pass (keeps barriers simple).
        // You can relax this later for subpass-like situations.
        std::unordered_map<uint32_t, int> rw;
        auto mark = [&](uint32_t id, int v) { rw[id] |= v; };
        for (auto& c : p.io.colors)       mark(c.img.id, 2);
        if (p.io.hasDepth)                mark(p.io.depth.img.id, 2);
        for (auto& s : p.io.writesStorage)mark(s.img.id, 2);
        for (auto& r : p.io.readsSampled) mark(r.img.id, 1);
        for (auto& r : p.io.readsStorage) mark(r.img.id, 1);
        for (auto& kv : rw) {
            if (kv.second == 3) {
                throw std::runtime_error("RenderGraph: same-pass read+write to an image is not supported (image id " + std::to_string(kv.first) + ")");
            }
        }
    }

    // --------------- Compile / Execute --------

    void RenderGraph::compile() {
        // Build preBarriers for each pass up-front (based on images_[].lastUse tracked as we walk).
        // We simulate execution order (insertion order) to know src→dst transitions per pass.
        // Make a temp copy of lastUse so we don’t mutate the “real” ones yet.
        std::vector<hvk::barrier::ImgUse> tmpLastUse(images_.size(), hvk::barrier::ImgUse::Undefined);
        for (size_t i = 0; i < images_.size(); ++i) tmpLastUse[i] = images_[i].lastUse;

        for (uint32_t pi = 0; pi < passes_.size(); ++pi) {
            auto& pass = passes_[pi];
            pass.preBarriers.clear();

            auto pushTrans = [&](uint32_t imgId, hvk::barrier::ImgUse dst) {
                auto& img = images_[imgId];
                auto prev = tmpLastUse[imgId];
                if (prev == dst) return;
                VkImageSubresourceRange range = hvk::barrier::full_range(aspect_from_format(img.format));
                pass.preBarriers.push_back(hvk::barrier::make_image_barrier(img.image, range, prev, dst));
                // Do NOT advance tmpLastUse here; we advance only on writes after pass completes below.
                };

            // Build transitions the same way execute() will use them
            for (auto& c : pass.io.colors) pushTrans(c.img.id, useForColorWrite());
            if (pass.io.hasDepth)          pushTrans(pass.io.depth.img.id, useForDepthWrite(pass.io.depth.readOnly));
            for (auto& r : pass.io.readsSampled)  pushTrans(r.img.id, useForSampledRead());
            for (auto& r : pass.io.readsStorage)  pushTrans(r.img.id, useForStorageRead());
            for (auto& w : pass.io.writesStorage) pushTrans(w.img.id, useForStorageWrite());

            // Writes update tmpLastUse (future passes "see" the new layout)
            for (auto& c : pass.io.colors)        tmpLastUse[c.img.id] = useForColorWrite();
            if (pass.io.hasDepth)                 tmpLastUse[pass.io.depth.img.id] = useForDepthWrite(pass.io.depth.readOnly);
            for (auto& w : pass.io.writesStorage) tmpLastUse[w.img.id] = useForStorageWrite();

            // Present intents: just mark; we’ll place transitions after all passes
            for (auto& pr : pass.io.presents) markForPresent(pr.img);
        }
    }

    void RenderGraph::execute(CmdList& cmd) {
        for (auto& pass : passes_) {
            // apply precomputed barriers
            applyBarriers(cmd, pass.preBarriers);

            DebugUtils::CmdLabelScope labelScope(
                (autoLabels_ ? dbg_ : nullptr), cmd.handle(), pass.name.c_str(), nullptr);

            const bool hasColor = !pass.io.colors.empty();
            const bool hasDepth = pass.io.hasDepth;
            const bool graphics = (pass.kind == PassKind::Graphics);

            if (graphics && (hasColor || hasDepth)) {
                std::vector<CmdList::ColorAttachment> cols;
                cols.reserve(pass.io.colors.size());
                for (size_t i = 0; i < pass.io.colors.size(); ++i) {
                    const auto& c = pass.io.colors[i];
                    CmdList::ColorAttachment ca{};
                    ca.view = pass.colorViews[i];
                    ca.layout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL;
                    ca.loadOp = c.load;
                    ca.storeOp = c.store;
                    ca.clear = c.clear;
                    cols.push_back(ca);
                }
                CmdList::DepthAttachment depth{};
                const CmdList::DepthAttachment* pDepth = nullptr;
                if (hasDepth) {
                    const auto& d = pass.io.depth;
                    depth.view = pass.depthView;
                    depth.layout = d.readOnly ? VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
                        : VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                    depth.loadOp = d.load;
                    depth.storeOp = d.store;
                    depth.clear = d.clear;
                    pDepth = &depth;
                }
                VkRect2D area{ {0,0}, pass.extent };
                cmd.beginRendering(area, cols, pDepth ? &depth : nullptr, 0);
                cmd.setViewportScissor(pass.extent, /*yDown*/false);
            }

            // user recording
            PassContext ctx;
            ctx.extent_ = pass.extent;
            ctx.colorViews_ = pass.colorViews;
            ctx.depthView_ = pass.depthView;
            pass.record(cmd, ctx);

            if (graphics && (hasColor || hasDepth)) {
                cmd.endRendering();
            }

            // advance lastUse for writers (mirror compile() behavior)
            for (auto& c : pass.io.colors)        images_[c.img.id].lastUse = useForColorWrite();
            if (pass.io.hasDepth)                 images_[pass.io.depth.img.id].lastUse = useForDepthWrite(pass.io.depth.readOnly);
            for (auto& w : pass.io.writesStorage) images_[w.img.id].lastUse = useForStorageWrite();
        }

        // Final present transitions
        hvk::barrier::Batch b;
        for (uint32_t i = 0; i < images_.size(); ++i) {
            if (!finalPresent_[i]) continue;
            auto& img = images_[i];
            auto mb = hvk::barrier::make_image_barrier(
                img.image,
                hvk::barrier::full_range(aspect_from_format(img.format)),
                img.lastUse, hvk::barrier::ImgUse::Present);
            b.imgs.push_back(mb);
            img.lastUse = hvk::barrier::ImgUse::Present;
        }
        hvk::barrier::submit(cmd.handle(), b);
    }

    // --------------- Helpers ------------------

    VkImageView RenderGraph::imageView(ImageHandle h) const {
        if (!h.valid() || h.id >= images_.size()) return VK_NULL_HANDLE;
        return images_[h.id].view;
    }
    VkFormat RenderGraph::imageFormat(ImageHandle h) const {
        if (!h.valid() || h.id >= images_.size()) return VK_FORMAT_UNDEFINED;
        return images_[h.id].format;
    }
    VkExtent2D RenderGraph::imageExtent(ImageHandle h) const {
        if (!h.valid() || h.id >= images_.size()) return VkExtent2D{ 0,0 };
        return images_[h.id].extent;
    }

    void RenderGraph::destroyCurrentFrameResources() {
        for (auto& img : images_) {
            if (!img.isExternal) {
                img.ownedView.destroy();
                img.ownedImage.destroy();
                img.image = VK_NULL_HANDLE;
                img.view = VK_NULL_HANDLE;
                img.lastUse = hvk::barrier::ImgUse::Undefined;
            }
        }
    }

    VkExtent2D RenderGraph::resolveSize(const Size2D& s) const {
        if (s.cls == Size2D::Class::Absolute) return { s.width, s.height };
        uint32_t w = (swapExtent_.width * s.scaleNum) / s.scaleDen;
        uint32_t h = (swapExtent_.height * s.scaleNum) / s.scaleDen;
        return VkExtent2D{ std::max(1u,w), std::max(1u,h) };
    }

    void RenderGraph::realizeTransientImage(uint32_t idx) {
        Img& img = images_[idx];
        if (img.image) return;

        const auto& d = img.desc;
        VkImageUsageFlags usage = 0;
        if (d.canColor)       usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if (d.canDepth)       usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        if (d.canSample)      usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        if (d.canStorage)     usage |= VK_IMAGE_USAGE_STORAGE_BIT;
        if (d.canTransferSrc) usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if (d.canTransferDst) usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

        GpuImageCreateInfo ci{};
        ci.device = dev_;
        ci.format = d.format;
        ci.width = img.extent.width;
        ci.height = img.extent.height;
        ci.depth = 1;
        ci.mipLevels = d.mipLevels;
        ci.arrayLayers = d.arrayLayers;
        ci.samples = d.samples;
        ci.usage = usage;
        ci.flags = 0;
        ci.memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        ci.allocFlags = VMA_ALLOCATION_CREATE_STRATEGY_BEST_FIT_BIT;
        ci.debugName = d.debugName.empty() ? std::string("rg.image") : d.debugName;

        img.ownedImage.create(ci);
        img.image = img.ownedImage.handle();
        img.format = d.format;

        const VkImageAspectFlags aspect = aspect_from_format(d.format);
        auto range = hvk_full_range(aspect, d.mipLevels ? d.mipLevels : 1, d.arrayLayers ? d.arrayLayers : 1);
        img.ownedView = img.ownedImage.makeView(VK_IMAGE_VIEW_TYPE_2D, aspect, range, ci.debugName + ".view");
        img.view = img.ownedView.handle();

        img.lastUse = hvk::barrier::ImgUse::Undefined;
    }

    void RenderGraph::buildBarriersForPass(uint32_t, std::vector<VkImageMemoryBarrier2>&) const {
        // kept for compatibility; replaced by preBarriers built in compile()
    }

    void RenderGraph::applyBarriers(CmdList& cmd, const std::vector<VkImageMemoryBarrier2>& imgBarriers) const {
        if (imgBarriers.empty()) return;
        hvk::barrier::Batch b;
        b.imgs = imgBarriers;
        hvk::barrier::submit(cmd.handle(), b);
    }

} // namespace hvk
