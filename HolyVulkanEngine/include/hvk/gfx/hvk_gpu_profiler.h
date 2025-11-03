/**
 * @file hvk_gpu_profiler.h
 * @brief GPU timestamp-based performance profiling
 * @author Holy Vulkan Engine
 * @date 2025
 * Records GPU timestamp queries for named scopes and resolves them to
 * millisecond timings for performance analysis.
 */

#ifndef HVK_GPU_PROFILER_H
#define HVK_GPU_PROFILER_H

#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <cstdint>

namespace hvk {

    class Device; // fwd
    class DebugUtils; // fwd

    struct GpuProfilerCreateInfo {
        const Device* device = nullptr;      // required
        uint32_t      framesInFlight = 2;    // 23 typical
        uint32_t      queriesPerFrame = 256; // must fit 2*scopes + standalone stamps
        std::string   debugBaseName{};       // optional
    };

    // Per-frame timestamp range result
    struct GpuRangeResult {
        std::string name;
        double      milliseconds = 0.0;
        uint64_t    ticksStart = 0;
        uint64_t    ticksEnd = 0;
    };

    // Records scope begin/end and resolves to milliseconds next frame.
    class GpuProfiler {
    public:
        GpuProfiler() = default;
        explicit GpuProfiler(const GpuProfilerCreateInfo& ci);
        ~GpuProfiler();

        GpuProfiler(const GpuProfiler&) = delete;
        GpuProfiler& operator=(const GpuProfiler&) = delete;

        GpuProfiler(GpuProfiler&&) noexcept;
        GpuProfiler& operator=(GpuProfiler&&) noexcept;

        // Frame lifecycle ---------------------------------------------------------
        // Call at the start of a frame slot after you've waited on its fence/timeline.
        void beginFrame(uint32_t frameIndex);

        // Scoped range API --------------------------------------------------------
        // Begin a named range; returns a token to pass to endRange().
        // Also begins a debug label with the same name (if DebugUtils provided).
        uint32_t beginRange(VkCommandBuffer cmd, const char* name,
            const DebugUtils* dbg = nullptr, const float rgba[4] = nullptr);

        // End a previously begun range by token.
        // Also ends the debug label if one was opened via beginRange().
        void endRange(VkCommandBuffer cmd, uint32_t token);

        // Optional: standalone timestamp (no name, no result entry).
        void writeStamp(VkCommandBuffer cmd);

        // Resolve results for the last fully retired frame (usually the one you
        // called beginFrame() on at the start of this frame). Returns false if
        // no ranges recorded. Results are ordered by begin token.
        bool resolve(uint32_t frameIndexJustRetired, std::vector<GpuRangeResult>& outResults);

        // True if timestamp writes are operational.
        bool supported() const { return ok_; }

        // Timestamp period in *nanoseconds per tick* (from device props).
        double timestampPeriodNs() const { return periodNs_; }

    private:
        struct Range {
            uint32_t startQuery = ~0u;
            uint32_t endQuery = ~0u;
            std::string name;
            bool hadDebugLabel = false;
        };
        struct PerFrame {
            VkQueryPool pool = VK_NULL_HANDLE;
            uint32_t    used = 0;         // queries used this frame
            std::vector<Range> ranges;    // named ranges (order of begin)
        };

        void destroy();
        void createPools();
        void resetFramePF(PerFrame& pf) const;
        uint32_t allocQuery(uint32_t frame);

        // timestamp write helpers (sync2 or classic)
        void cmdWriteTimestamp(VkCommandBuffer cmd, VkQueryPool pool, uint32_t query) const;

    private:
        const Device* device_ = nullptr;
        uint32_t frames_ = 0;
        uint32_t queriesPerFrame_ = 0;
        double   periodNs_ = 0.0;

        std::vector<PerFrame> perFrame_;
        uint32_t cur_ = 0;

        // feature flags / function pointers
        bool ok_ = false;
        bool hasWriteTimestamp2_ = false;

        PFN_vkCmdWriteTimestamp2        pWriteTs2_ = nullptr;
        PFN_vkResetQueryPool            pResetQP_ = nullptr; // core in 1.2+
    };

    ///////////////////////////////////////////////////////////////////////////////
    // RAII scope helper: begins a GPU range (and a debug label) on ctor and ends
    // it on dtor.
    class GpuTimerScope {
    public:
        GpuTimerScope(GpuProfiler& p, VkCommandBuffer cmd,
            const char* name, const DebugUtils* du = nullptr, const float rgba[4] = nullptr)
            : profiler(p), cmd(cmd) {
            token = profiler.beginRange(cmd, name, du, rgba);
        }
        ~GpuTimerScope() {
            if (token != ~0u) profiler.endRange(cmd, token);
        }
        GpuTimerScope(const GpuTimerScope&) = delete;
        GpuTimerScope& operator=(const GpuTimerScope&) = delete;
    private:
        GpuProfiler& profiler;
        VkCommandBuffer cmd{ VK_NULL_HANDLE };
        uint32_t        token = ~0u;
    };

} // namespace hvk

#endif // HVK_GPU_PROFILER_H
