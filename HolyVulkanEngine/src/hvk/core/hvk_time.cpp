#include <hvk/core/hvk_time.hpp>
#include <algorithm>

namespace hvk {

// --- Static member initialization ---
Time::TimePoint Time::startTime_;
Time::TimePoint Time::lastFrameTime_;
Time::TimePoint Time::currentFrameTime_;
float Time::deltaTime_ = 0.0f;
float Time::totalTime_ = 0.0f;
float Time::unscaledDeltaTime_ = 0.0f;
float Time::unscaledTotalTime_ = 0.0f;
uint64_t Time::frameCount_ = 0;
float Time::timeScale_ = 1.0f;
float Time::fps_ = 0.0f;
float Time::averageFrameTimeMs_ = 0.0f;
float Time::fpsAccumulator_ = 0.0f;
uint32_t Time::fpsFrameCount_ = 0;
Time::TimePoint Time::fpsLastUpdateTime_;

// --- Initialization ---

void Time::init() {
    startTime_ = Clock::now();
    lastFrameTime_ = startTime_;
    currentFrameTime_ = startTime_;
    fpsLastUpdateTime_ = startTime_;

    deltaTime_ = 0.0f;
    totalTime_ = 0.0f;
    unscaledDeltaTime_ = 0.0f;
    unscaledTotalTime_ = 0.0f;
    frameCount_ = 0;
    timeScale_ = 1.0f;
    fps_ = 0.0f;
    averageFrameTimeMs_ = 0.0f;
    fpsAccumulator_ = 0.0f;
    fpsFrameCount_ = 0;
}

// --- Update ---

void Time::update() {
    // Update time points
    lastFrameTime_ = currentFrameTime_;
    currentFrameTime_ = Clock::now();

    // Compute unscaled delta time
    Duration frameDuration = currentFrameTime_ - lastFrameTime_;
    unscaledDeltaTime_ = frameDuration.count();

    // Clamp delta time to prevent spiral of death (max 0.1 seconds = 10 FPS minimum)
    unscaledDeltaTime_ = std::min(unscaledDeltaTime_, 0.1f);

    // Apply time scale
    deltaTime_ = unscaledDeltaTime_ * timeScale_;

    // Update total time
    unscaledTotalTime_ += unscaledDeltaTime_;
    totalTime_ += deltaTime_;

    // Increment frame count
    ++frameCount_;

    // Update FPS counter (every second)
    fpsAccumulator_ += unscaledDeltaTime_;
    ++fpsFrameCount_;

    Duration fpsDuration = currentFrameTime_ - fpsLastUpdateTime_;
    float fpsDurationSeconds = fpsDuration.count();

    if (fpsDurationSeconds >= 1.0f) {
        // Compute FPS and average frame time
        fps_ = static_cast<float>(fpsFrameCount_) / fpsDurationSeconds;
        averageFrameTimeMs_ = (fpsAccumulator_ / static_cast<float>(fpsFrameCount_)) * 1000.0f;

        // Reset accumulators
        fpsAccumulator_ = 0.0f;
        fpsFrameCount_ = 0;
        fpsLastUpdateTime_ = currentFrameTime_;
    }
}

} // namespace hvk
