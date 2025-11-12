/**
 * @file hvk_global_descriptors.hpp
 * @brief Global descriptor set management
 * @author Holy Vulkan Engine
 * @date 2025
 * Manages Set 0 descriptors for scene, camera, and lighting data.
 */

#ifndef HVK_GLOBAL_DESCRIPTORS_HPP
#define HVK_GLOBAL_DESCRIPTORS_HPP

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_descriptors.h>
#include <hvk/gfx/hvk_gpu_resources.h>
#include <hvk/gfx/hvk_scene_data.hpp>
#include <hvk/gfx/hvk_camera_data.hpp>
#include <hvk/gfx/hvk_light_data.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <deque>

namespace hvk {

/**
 * GlobalDescriptorLayout - Creates and manages Set 0 descriptor layout
 *
 * Set 0 bindings:
 *   Binding 0: SceneData  (UBO, std140)
 *   Binding 1: CameraData (UBO, std140)
 *   Binding 2: LightBuffer (SSBO, std430)
 *
 * All bindings are available in VERTEX | FRAGMENT | COMPUTE stages
 */
class GlobalDescriptorLayout {
public:
    /**
     * Create the Set 0 descriptor layout
     *
     * @param device Device reference
     * @return DescriptorSetLayout for Set 0
     */
    static DescriptorSetLayout create(const Device& device) {
        DescriptorSetLayoutCreateInfo layoutCI{};
        layoutCI.device = &device;

        // Binding 0: SceneData (UBO)
        layoutCI.bindings.push_back({
            /*binding*/    0,
            /*type*/       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            /*count*/      1,
            /*stages*/     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            /*samplers*/   nullptr
        });

        // Binding 1: CameraData (UBO)
        layoutCI.bindings.push_back({
            /*binding*/    1,
            /*type*/       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            /*count*/      1,
            /*stages*/     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            /*samplers*/   nullptr
        });

        // Binding 2: LightBuffer (SSBO)
        layoutCI.bindings.push_back({
            /*binding*/    2,
            /*type*/       VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            /*count*/      1,
            /*stages*/     VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT,
            /*samplers*/   nullptr
        });

        layoutCI.debugName = "set0_global";

        return DescriptorSetLayout(layoutCI);
    }

    /**
     * Get pool sizes for allocating descriptor sets with this layout
     *
     * @param setCount Number of descriptor sets to allocate
     * @return Vector of pool sizes
     */
    static std::vector<VkDescriptorPoolSize> getPoolSizes(uint32_t setCount) {
        return {
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, setCount * 2 },  // 2 UBOs per set
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, setCount * 1 }   // 1 SSBO per set
        };
    }
};

// ============================================================================
// Descriptor binding traits (compile-time binding specification)
// ============================================================================

/**
 * @brief Compile-time descriptor binding information
 *
 * Specializations define binding index and descriptor type for each data type.
 * This ensures type-safe descriptor writes with compile-time binding validation.
 */
template<typename DataT> struct DescriptorBinding;

template<> struct DescriptorBinding<SceneData> {
    static constexpr uint32_t binding = 0;
    static constexpr VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
};

template<> struct DescriptorBinding<CameraData> {
    static constexpr uint32_t binding = 1;
    static constexpr VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
};

template<> struct DescriptorBinding<LightBuffer> {
    static constexpr uint32_t binding = 2;
    static constexpr VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
};

// ============================================================================

/**
 * GlobalDescriptorWriter - Helper for writing Set 0 descriptors
 *
 * Now with template-based type-safe writes using compile-time binding traits!
 *
 * Usage:
 *   GlobalDescriptorWriter writer;
 *   writer.write<SceneData>(descSet, sceneBuffer);     // NEW: Type-safe!
 *   writer.write<CameraData>(descSet, cameraBuffer);   // NEW: Type-safe!
 *   writer.write<LightBuffer>(descSet, lightBuffer);   // NEW: Type-safe!
 *   writer.commit(device);
 *
 * Legacy methods also available: writeSceneData(), writeCameraData(), writeLightBuffer()
 */
class GlobalDescriptorWriter {
public:
    GlobalDescriptorWriter() = default;

    /**
     * Write SceneData uniform buffer (binding 0)
     *
     * @param set Descriptor set to write to
     * @param buffer Buffer containing SceneData
     * @param offset Offset in buffer
     * @param range Size of data (0 = whole buffer, or sizeof(SceneData))
     */
    void writeSceneData(VkDescriptorSet set, VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = sizeof(SceneData)) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = 0;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        // Use deque to keep addresses stable across push_back
        bufferInfos_.push_back(bufferInfo);
        writes_.push_back(write);
        writes_.back().pBufferInfo = &bufferInfos_.back();
    }

    /**
     * Write CameraData uniform buffer (binding 1)
     */
    void writeCameraData(VkDescriptorSet set, VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = sizeof(CameraData)) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = 1;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        bufferInfos_.push_back(bufferInfo);
        writes_.push_back(write);
        writes_.back().pBufferInfo = &bufferInfos_.back();
    }

    /**
     * Write LightBuffer storage buffer (binding 2)
     */
    void writeLightBuffer(VkDescriptorSet set, VkBuffer buffer, VkDeviceSize offset = 0, VkDeviceSize range = VK_WHOLE_SIZE) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = buffer;
        bufferInfo.offset = offset;
        bufferInfo.range = range;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = 2;
        write.dstArrayElement = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfo;

        bufferInfos_.push_back(bufferInfo);
        writes_.push_back(write);
        writes_.back().pBufferInfo = &bufferInfos_.back();
    }

    /**
     * @brief Template-based type-safe descriptor write (NEW & RECOMMENDED)
     *
     * Uses compile-time binding traits for type safety and clarity.
     * Wrong binding? Compile error! No runtime overhead.
     *
     * @tparam DataT Data type (SceneData, CameraData, or LightBuffer)
     * @param set Descriptor set to write to
     * @param buffer Buffer containing data
     * @param offset Offset in buffer
     * @param range Size of data (defaults to sizeof(DataT) or VK_WHOLE_SIZE for LightBuffer)
     *
     * Example:
     *   writer.write<SceneData>(set, sceneBuffer);
     *   writer.write<CameraData>(set, cameraBuffer);
     */
    template<typename DataT>
    void write(VkDescriptorSet set, VkBuffer buffer,
               VkDeviceSize offset = 0,
               VkDeviceSize range = 0) {
        // Auto-deduce range if not specified
        if (range == 0) {
            range = std::is_same_v<DataT, LightBuffer> ? VK_WHOLE_SIZE : sizeof(DataT);
        }

        VkDescriptorBufferInfo bufferInfo{buffer, offset, range};
        bufferInfos_.push_back(bufferInfo);

        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = set;
        write.dstBinding = DescriptorBinding<DataT>::binding;  // Compile-time binding!
        write.dstArrayElement = 0;
        write.descriptorType = DescriptorBinding<DataT>::type; // Compile-time type!
        write.descriptorCount = 1;
        write.pBufferInfo = &bufferInfos_.back();

        writes_.push_back(write);
    }

    /**
     * Commit all writes to the GPU
     */
    void commit(VkDevice device) {
        if (!writes_.empty()) {
            vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes_.size()), writes_.data(), 0, nullptr);
        }
        clear();
    }

    /**
     * Clear pending writes (called automatically after commit)
     */
    void clear() {
        writes_.clear();
        bufferInfos_.clear();
    }

private:
    std::vector<VkWriteDescriptorSet> writes_;
    // deque ensures pointers to elements remain valid across push_back
    std::deque<VkDescriptorBufferInfo> bufferInfos_;
};

/**
 * GlobalDescriptorSet - RAII wrapper for Set 0 with per-frame resources
 *
 * Manages SceneData, CameraData, and LightBuffer for each frame in flight.
 * Automatically creates buffers and descriptor sets.
 *
 * Usage:
 *   GlobalDescriptorSet globalDesc(device, allocator, layout, 3); // 3 frames
 *
 *   // Each frame:
 *   globalDesc.updateScene(frameIdx, sceneData);
 *   globalDesc.updateCamera(frameIdx, cameraData);
 *   globalDesc.updateLights(frameIdx, lightBuffer);
 *
 *   // Rendering:
 *   vkCmdBindDescriptorSets(..., globalDesc.get(frameIdx), ...);
 */
class GlobalDescriptorSet {
public:
    GlobalDescriptorSet() = default;

    GlobalDescriptorSet(
        const Device& device,
        DescriptorAllocator& allocator,
        const DescriptorSetLayout& layout,
        uint32_t framesInFlight
    ) {
        init(device, allocator, layout, framesInFlight);
    }

    ~GlobalDescriptorSet() {
        destroy();
    }

    // Move-only
    GlobalDescriptorSet(const GlobalDescriptorSet&) = delete;
    GlobalDescriptorSet& operator=(const GlobalDescriptorSet&) = delete;
    GlobalDescriptorSet(GlobalDescriptorSet&& o) noexcept { move_from(std::move(o)); }
    GlobalDescriptorSet& operator=(GlobalDescriptorSet&& o) noexcept {
        if (this != &o) { destroy(); move_from(std::move(o)); }
        return *this;
    }

    void init(
        const Device& device,
        DescriptorAllocator& allocator,
        const DescriptorSetLayout& layout,
        uint32_t framesInFlight
    );

    void destroy();

    // Update scene data for a frame
    void updateScene(uint32_t frameIndex, const SceneData& data);

    // Update camera data for a frame
    void updateCamera(uint32_t frameIndex, const CameraData& data);

    // Update light buffer for a frame
    void updateLights(uint32_t frameIndex, const LightBuffer& lights);

    // Get descriptor set for a frame
    VkDescriptorSet get(uint32_t frameIndex) const {
        return frameIndex < perFrame_.size() ? perFrame_[frameIndex].descriptorSet : VK_NULL_HANDLE;
    }

    uint32_t frameCount() const { return static_cast<uint32_t>(perFrame_.size()); }

private:
    struct PerFrame {
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        GpuBuffer sceneBuffer;
        GpuBuffer cameraBuffer;
        GpuBuffer lightBuffer;
        void* sceneMapped = nullptr;
        void* cameraMapped = nullptr;
        void* lightMapped = nullptr;
        size_t lightBufferCapacity = 0; // in bytes
    };

    void move_from(GlobalDescriptorSet&& o) noexcept;

    const Device* device_ = nullptr;
    std::vector<PerFrame> perFrame_;
};

} // namespace hvk

#endif // HVK_GLOBAL_DESCRIPTORS_HPP
