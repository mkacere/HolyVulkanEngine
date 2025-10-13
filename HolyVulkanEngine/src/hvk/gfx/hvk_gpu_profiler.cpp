#include "pch.h"

#include <hvk/gfx/hvk_gpu_profiler.h>
#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_debug_utils.h>

#ifndef VK_CHECK
#define VK_CHECK(x) do { VkResult _e = (x); if (_e != VK_SUCCESS) throw std::runtime_error("Vulkan error: " #x); } while(0)
#endif

namespace hvk {

    GpuProfiler::GpuProfiler(const GpuProfilerCreateInfo& ci)
        : device_(ci.device)
        , frames_(ci.framesInFlight ? ci.framesInFlight : 2)
        , queriesPerFrame_(ci.queriesPerFrame ? ci.queriesPerFrame : 256)
    {
        if (!device_) throw std::invalid_argument("GpuProfiler: device is null");
        periodNs_ = static_cast<double>(device_->properties().limits.timestampPeriod);

        // Function pointers
        pWriteTs2_ = reinterpret_cast<PFN_vkCmdWriteTimestamp2>(
            vkGetDeviceProcAddr(device_->device(), "vkCmdWriteTimestamp2"));
        pResetQP_ = reinterpret_cast<PFN_vkResetQueryPool>(
            vkGetDeviceProcAddr(device_->device(), "vkResetQueryPool"));

        hasWriteTimestamp2_ = (pWriteTs2_ != nullptr);

        perFrame_.resize(frames_);
        createPools();
        ok_ = true; // if pool creation succeeded, we’re operational
    }

    GpuProfiler::~GpuProfiler() { destroy(); }

    GpuProfiler::GpuProfiler(GpuProfiler&& o) noexcept { *this = std::move(o); }

    GpuProfiler& GpuProfiler::operator=(GpuProfiler&& o) noexcept {
        if (this != &o) {
            destroy();
            device_ = o.device_; o.device_ = nullptr;
            frames_ = o.frames_; o.frames_ = 0;
            queriesPerFrame_ = o.queriesPerFrame_; o.queriesPerFrame_ = 0;
            periodNs_ = o.periodNs_; o.periodNs_ = 0;
            perFrame_ = std::move(o.perFrame_);
            cur_ = o.cur_; o.cur_ = 0;
            ok_ = o.ok_; o.ok_ = false;
            hasWriteTimestamp2_ = o.hasWriteTimestamp2_;
            pWriteTs2_ = o.pWriteTs2_; o.pWriteTs2_ = nullptr;
            pResetQP_ = o.pResetQP_;  o.pResetQP_ = nullptr;
        }
        return *this;
    }

    void GpuProfiler::destroy() {
        if (!device_) return;
        for (auto& pf : perFrame_) {
            if (pf.pool) vkDestroyQueryPool(device_->device(), pf.pool, nullptr);
            pf.pool = VK_NULL_HANDLE;
            pf.used = 0;
            pf.ranges.clear();
        }
        perFrame_.clear();
        ok_ = false;
    }

    void GpuProfiler::createPools() {
        for (uint32_t i = 0; i < frames_; ++i) {
            VkQueryPoolCreateInfo qi{ VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
            qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
            qi.queryCount = queriesPerFrame_;
            VK_CHECK(vkCreateQueryPool(device_->device(), &qi, nullptr, &perFrame_[i].pool));

            // Optional object names
            device_->setObjectName(VK_OBJECT_TYPE_QUERY_POOL,
                reinterpret_cast<uint64_t>(perFrame_[i].pool),
                "gpu_prof/timestamps#" + std::to_string(i));
        }
    }

    void GpuProfiler::resetFramePF(PerFrame& pf) const {
        // Reset the whole pool for the frame; core function since 1.2
        if (pResetQP_) {
            pResetQP_(device_->device(), pf.pool, 0, queriesPerFrame_);
        }
        else {
            // Fallback: overwrite queries (not ideal), but on 1.4 you should always have reset.
        }
    }

    void GpuProfiler::beginFrame(uint32_t frameIndex) {
        if (!ok_) return;
        cur_ = frameIndex % frames_;
        PerFrame& pf = perFrame_[cur_];
        resetFramePF(pf);
        pf.used = 0;
        pf.ranges.clear();
    }

    uint32_t GpuProfiler::allocQuery(uint32_t frame) {
        PerFrame& pf = perFrame_[frame];
        if (pf.used >= queriesPerFrame_)
            throw std::runtime_error("GpuProfiler: out of timestamp queries this frame");
        return pf.used++;
    }

    void GpuProfiler::cmdWriteTimestamp(VkCommandBuffer cmd, VkQueryPool pool, uint32_t query) const {
        if (hasWriteTimestamp2_) {
            // Use sync2 path; BOTTOM_OF_PIPE is a sensible default for timing markers.
            pWriteTs2_(cmd, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT, pool, query);
        }
        else {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, pool, query);
        }
    }

    uint32_t GpuProfiler::beginRange(VkCommandBuffer cmd, const char* name,
        const DebugUtils* dbg, const float rgba[4]) {
        if (!ok_) return ~0u;

        PerFrame& pf = perFrame_[cur_];

        // Allocate start query
        const uint32_t qStart = allocQuery(cur_);
        cmdWriteTimestamp(cmd, pf.pool, qStart);

        // Record range meta
        pf.ranges.push_back({});
        Range& r = pf.ranges.back();
        r.startQuery = qStart;
        r.name = name ? std::string(name) : std::string();
        r.hadDebugLabel = (dbg != nullptr);

        // Optional debug label
        static const float defaultRgba[4] = { 0.2f, 0.6f, 0.9f, 1.0f };
        if (dbg) dbg->cmdBeginLabel(cmd, r.name, rgba ? rgba : defaultRgba);

        return static_cast<uint32_t>(pf.ranges.size() - 1);
    }

    void GpuProfiler::endRange(VkCommandBuffer cmd, uint32_t token) {
        if (!ok_ || token == ~0u) return;

        PerFrame& pf = perFrame_[cur_];
        if (token >= pf.ranges.size())
            return;

        Range& r = pf.ranges[token];

        // Allocate end query
        const uint32_t qEnd = allocQuery(cur_);
        cmdWriteTimestamp(cmd, pf.pool, qEnd);
        r.endQuery = qEnd;

        // Close debug label if we opened one at beginRange()
        if (r.hadDebugLabel) {
            // We can’t be sure which DebugUtils was used; the end call is stateless per-cmd, so it’s fine.
            // Call via device dispatch through a fresh helper if you want; here it will be called by caller’s scope.
            // Do nothing here; the RAII scope variant will close. In beginRange(), we only opened if dbg!=nullptr.
        }
    }

    void GpuProfiler::writeStamp(VkCommandBuffer cmd) {
        if (!ok_) return;
        PerFrame& pf = perFrame_[cur_];
        const uint32_t q = allocQuery(cur_);
        cmdWriteTimestamp(cmd, pf.pool, q);
    }

    bool GpuProfiler::resolve(uint32_t frameIndexJustRetired, std::vector<GpuRangeResult>& outResults) {
        if (!ok_) return false;

        const uint32_t f = frameIndexJustRetired % frames_;
        PerFrame& pf = perFrame_[f];
        if (pf.used == 0 || pf.ranges.empty()) {
            outResults.clear();
            return false;
        }

        // Read back all used queries as 64-bit.
        std::vector<uint64_t> ticks(pf.used, 0);
        VkResult gr = vkGetQueryPoolResults(
            device_->device(), pf.pool,
            0, pf.used,
            sizeof(uint64_t) * pf.used, ticks.data(),
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

        if (gr != VK_SUCCESS) return false;

        outResults.clear();
        outResults.reserve(pf.ranges.size());

        const double nsPerTick = periodNs_;
        for (const Range& r : pf.ranges) {
            if (r.startQuery == ~0u || r.endQuery == ~0u) continue;
            uint64_t t0 = ticks[r.startQuery];
            uint64_t t1 = ticks[r.endQuery];
            if (t1 < t0) continue; // guard
            double ms = (static_cast<double>(t1 - t0) * nsPerTick) / 1.0e6;
            outResults.push_back({ r.name, ms, t0, t1 });
        }
        return !outResults.empty();
    }

} // namespace hvk
