#include <hvk/gfx/hvk_global_descriptors.hpp>
#include <hvk/gfx/hvk_utils.hpp>
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace hvk {

void GlobalDescriptorSet::init(
    const Device& device,
    DescriptorAllocator& allocator,
    const DescriptorSetLayout& layout,
    uint32_t framesInFlight
) {
    if (framesInFlight == 0) {
        throw std::invalid_argument("GlobalDescriptorSet: framesInFlight must be > 0");
    }

    device_ = &device;
    perFrame_.resize(framesInFlight);

    // Allocate descriptor sets
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        perFrame_[i].descriptorSet = allocator.allocate(layout);
    }

    // Create buffers for each frame
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        PerFrame& pf = perFrame_[i];

        // SceneData buffer (host-visible, persistently mapped)
        pf.sceneBuffer = GpuBuffer({
            .device = device_,
            .size = sizeof(SceneData),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
            .persistentMap = true,
            .debugName = "scene_ubo_frame" + std::to_string(i)
        });
        pf.sceneMapped = pf.sceneBuffer.mapped();

        // CameraData buffer (host-visible, persistently mapped)
        pf.cameraBuffer = GpuBuffer({
            .device = device_,
            .size = sizeof(CameraData),
            .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
            .persistentMap = true,
            .debugName = "camera_ubo_frame" + std::to_string(i)
        });
        pf.cameraMapped = pf.cameraBuffer.mapped();

        // LightBuffer SSBO (host-visible, persistently mapped, starts with capacity for 128 lights)
        constexpr size_t initialLightCount = 128;
        pf.lightBufferCapacity = 16 + initialLightCount * sizeof(Light); // header + lights
        pf.lightBuffer = GpuBuffer({
            .device = device_,
            .size = pf.lightBufferCapacity,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
            .persistentMap = true,
            .debugName = "light_ssbo_frame" + std::to_string(i)
        });
        pf.lightMapped = pf.lightBuffer.mapped();

        // Write descriptors
        GlobalDescriptorWriter writer;
        writer.writeSceneData(pf.descriptorSet, pf.sceneBuffer.handle(), 0, sizeof(SceneData));
        writer.writeCameraData(pf.descriptorSet, pf.cameraBuffer.handle(), 0, sizeof(CameraData));
        writer.writeLightBuffer(pf.descriptorSet, pf.lightBuffer.handle(), 0, VK_WHOLE_SIZE);
        writer.commit(device.device());
    }
}

void GlobalDescriptorSet::destroy() {
    perFrame_.clear();
    device_ = nullptr;
}

void GlobalDescriptorSet::updateScene(uint32_t frameIndex, const SceneData& data) {
    if (frameIndex >= perFrame_.size()) {
        throw std::out_of_range("GlobalDescriptorSet::updateScene: frameIndex out of range");
    }

    PerFrame& pf = perFrame_[frameIndex];
    if (!pf.sceneMapped) {
        throw std::runtime_error("GlobalDescriptorSet::updateScene: buffer not mapped");
    }

    std::memcpy(pf.sceneMapped, &data, sizeof(SceneData));
}

void GlobalDescriptorSet::updateCamera(uint32_t frameIndex, const CameraData& data) {
    if (frameIndex >= perFrame_.size()) {
        throw std::out_of_range("GlobalDescriptorSet::updateCamera: frameIndex out of range");
    }

    PerFrame& pf = perFrame_[frameIndex];
    if (!pf.cameraMapped) {
        throw std::runtime_error("GlobalDescriptorSet::updateCamera: buffer not mapped");
    }

    std::memcpy(pf.cameraMapped, &data, sizeof(CameraData));
}

void GlobalDescriptorSet::updateLights(uint32_t frameIndex, const LightBuffer& lights) {
    if (frameIndex >= perFrame_.size()) {
        throw std::out_of_range("GlobalDescriptorSet::updateLights: frameIndex out of range");
    }

    PerFrame& pf = perFrame_[frameIndex];
    if (!pf.lightMapped) {
        throw std::runtime_error("GlobalDescriptorSet::updateLights: buffer not mapped");
    }

    size_t requiredSize = lights.getBufferSize();

    // Grow buffer if needed
    if (requiredSize > pf.lightBufferCapacity) {
        // Recreate buffer with larger capacity
        size_t newCapacity = std::max(requiredSize, pf.lightBufferCapacity * 2);

        pf.lightBuffer = GpuBuffer({
            .device = device_,
            .size = newCapacity,
            .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .allocFlags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .memUsage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
            .persistentMap = true,
            .debugName = "light_ssbo_frame" + std::to_string(frameIndex) + "_resized"
        });
        pf.lightMapped = pf.lightBuffer.mapped();
        pf.lightBufferCapacity = newCapacity;

        // Update descriptor
        GlobalDescriptorWriter writer;
        writer.writeLightBuffer(pf.descriptorSet, pf.lightBuffer.handle(), 0, VK_WHOLE_SIZE);
        writer.commit(device_->device());
    }

    // Write light data
    lights.writeTo(pf.lightMapped);
}

void GlobalDescriptorSet::move_from(GlobalDescriptorSet&& o) noexcept {
    device_ = o.device_;
    perFrame_ = std::move(o.perFrame_);
    o.device_ = nullptr;
}

} // namespace hvk
