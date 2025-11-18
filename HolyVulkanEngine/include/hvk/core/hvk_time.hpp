/**
 * @file hvk_time.hpp
 * @brief Global time management system with delta time and FPS tracking
 * @author soyruz
 * @date 2025
 */

#ifndef HVK_TIME_HPP
#define HVK_TIME_HPP

#include <chrono>
#include <cstdint>

namespace hvk {

/**
 * Time - Global time management system
 *
 * Design principles:
 * - Static singleton pattern for global access
 * - High-resolution timing using std::chrono
 * - Call update() once per frame to compute deltaTime
 * - Thread-safe reads (no mutex needed since writes only happen in update())
 * - Provides both float (seconds) and uint64_t (frame count) time values
 *
 * Usage:
 *   Time::init();
 *
 *   // In main loop:
 *   Time::update();
 *
 *   // In systems:
 *   float dt = Time::deltaTime();
 *   float t = Time::totalTime();
 *   uint64_t frame = Time::frameCount();
 */
class Time {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::duration<float>;

    // --- Initialization ---

    /**
     * Initialize time system
     * Must be called before first update()
     */
    static void init();

    /**
     * Update time values (call once per frame at start of frame)
     * Computes deltaTime and updates totalTime and frameCount
     */
    static void update();

    // --- Time Queries ---

    /**
     * Get delta time since last frame in seconds
     *
     * @return Time elapsed since last update() call
     */
    static float deltaTime() { return deltaTime_; }

    /**
     * Get total time since init() in seconds
     *
     * @return Total elapsed time
     */
    static float totalTime() { return totalTime_; }

    /**
     * Get current frame count
     *
     * @return Number of frames since init()
     */
    static uint64_t frameCount() { return frameCount_; }

    /**
     * Get unscaled delta time (ignores time scale)
     *
     * @return Real delta time in seconds
     */
    static float unscaledDeltaTime() { return unscaledDeltaTime_; }

    /**
     * Get unscaled total time (ignores time scale)
     *
     * @return Real total time in seconds
     */
    static float unscaledTotalTime() { return unscaledTotalTime_; }

    // --- Time Scale ---

    /**
     * Set time scale factor
     * Affects deltaTime() and totalTime() but not unscaled versions
     *
     * @param scale Time scale (1.0 = normal, 0.5 = half speed, 2.0 = double speed)
     */
    static void setTimeScale(float scale) { timeScale_ = scale; }

    /**
     * Get current time scale
     *
     * @return Time scale factor
     */
    static float timeScale() { return timeScale_; }

    // --- FPS ---

    /**
     * Get current frames per second (smoothed over last second)
     *
     * @return Approximate FPS
     */
    static float fps() { return fps_; }

    /**
     * Get average frame time in milliseconds (smoothed over last second)
     *
     * @return Average frame time in ms
     */
    static float averageFrameTime() { return averageFrameTimeMs_; }

private:
    // Time points
    static TimePoint startTime_;
    static TimePoint lastFrameTime_;
    static TimePoint currentFrameTime_;

    // Time values (scaled)
    static float deltaTime_;
    static float totalTime_;

    // Time values (unscaled)
    static float unscaledDeltaTime_;
    static float unscaledTotalTime_;

    // Frame count
    static uint64_t frameCount_;

    // Time scale
    static float timeScale_;

    // FPS tracking
    static float fps_;
    static float averageFrameTimeMs_;
    static float fpsAccumulator_;
    static uint32_t fpsFrameCount_;
    static TimePoint fpsLastUpdateTime_;

    // Prevent instantiation
    Time() = delete;
};

} // namespace hvk

#endif // HVK_TIME_HPP
