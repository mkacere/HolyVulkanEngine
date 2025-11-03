/**
 * @file hvk_shader_reflect.h
 * @brief SPIR-V shader reflection and pipeline layout generation
 * @author Holy Vulkan Engine
 * @date 2025
 * Uses SPIRV-Reflect to automatically generate descriptor set layouts and
 * pipeline layouts from compiled shader modules.
 */

#ifndef HVK_SHADER_REFLECT_H
#define HVK_SHADER_REFLECT_H

#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include <string>
#include <string_view>
#include <unordered_map>
#include <optional>
#include <stdexcept>

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_descriptors.h>
#include <hvk/gfx/hvk_pipeline_layout_cache.h>
#include <hvk/gfx/hvk_graphics_pipeline_cache.h>

namespace hvk {

    // -------------------- RAII shader module -------------------------------------

    struct ShaderModuleCreateInfo {
        const Device* device = nullptr;           // required
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
        const uint32_t* spirv = nullptr;          // required (words)
        size_t spirvWordCount = 0;
        std::string entry = "main";
        std::string debugName{};
    };

    class ShaderModule {
    public:
        ShaderModule() = default;
        explicit ShaderModule(const ShaderModuleCreateInfo& ci) { init(ci); }
        ~ShaderModule() { destroy(); }

        ShaderModule(const ShaderModule&) = delete;
        ShaderModule& operator=(const ShaderModule&) = delete;

        ShaderModule(ShaderModule&& o) noexcept { move_from(std::move(o)); }
        ShaderModule& operator=(ShaderModule&& o) noexcept {
            if (this != &o) { destroy(); move_from(std::move(o)); }
            return *this;
        }

        void init(const ShaderModuleCreateInfo& ci);
        void destroy();

        VkShaderModule handle() const { return module_; }
        VkShaderStageFlagBits stage() const { return stage_; }
        const std::string& entry() const { return entry_; }
        const Device* device() const { return device_; }

    private:
        void move_from(ShaderModule&& o) noexcept {
            device_ = o.device_; o.device_ = nullptr;
            module_ = o.module_; o.module_ = VK_NULL_HANDLE;
            stage_ = o.stage_;  o.stage_ = VK_SHADER_STAGE_VERTEX_BIT;
            entry_ = std::move(o.entry_);
        }

    private:
        const Device* device_ = nullptr;
        VkShaderModule module_ = VK_NULL_HANDLE;
        VkShaderStageFlagBits stage_ = VK_SHADER_STAGE_VERTEX_BIT;
        std::string entry_ = "main";
    };

    // -------------------- Reflection model ---------------------------------------

    struct ReflectedBinding {
        uint32_t set = 0;
        uint32_t binding = 0;
        VkDescriptorType type = VK_DESCRIPTOR_TYPE_MAX_ENUM;
        uint32_t count = 1;                 // array length (1 if not array). Runtime array -> 0.
        VkShaderStageFlags stages = 0;      // OR across all stages that use it
        bool runtimeArray = false;          // true if unsized array (candidate for VARIABLE_DESCRIPTOR_COUNT)
    };

    struct ReflectedSet {
        uint32_t set = 0;
        std::vector<ReflectedBinding> bindings;
    };

    struct ReflectedPushConstant {
        uint32_t offset = 0;
        uint32_t size = 0;
        VkShaderStageFlags stages = 0;      // OR across all stages
    };

    struct ReflectedSpecConstant {
        uint32_t constantID = 0;
        uint32_t offset = 0;
        uint32_t size = 0;
    };

    struct ReflectedVertexAttr {
        uint32_t location = 0;
        VkFormat format = VK_FORMAT_UNDEFINED; // inferred from SPIR-V numeric type/vec size
        // App-defined fields (patch later):
        uint32_t binding = 0;
        uint32_t offset = 0;
    };

    struct ModuleReflection {
        VkShaderStageFlagBits stage = VK_SHADER_STAGE_VERTEX_BIT;
        std::vector<ReflectedSet> sets;           // may be empty
        std::vector<ReflectedPushConstant> pushes;// 0..N blocks (usually 0 or 1)
        std::vector<ReflectedSpecConstant> specs; // optional
        std::vector<ReflectedVertexAttr> vertexInputs; // only for VS (Builtins filtered out)
    };

    // -------------------- Overrides for policy flags ------------------------------

    struct BindingOverride {
        // identify
        uint32_t set = 0, binding = 0;
        // per-binding flags (optional OR of these)
        VkDescriptorBindingFlags flags = 0; // e.g., PARTIALLY_BOUND | UPDATE_AFTER_BIND | VARIABLE_DESCRIPTOR_COUNT
        // If VARIABLE_DESCRIPTOR_COUNT is set, you may also want to declare the max descriptor count for layout creation.
        std::optional<uint32_t> forceDescriptorCount; // if set, overrides reflected array length (or runtime array 0)
    };
    struct SetOverride {
        uint32_t set = 0;
        bool updateAfterBindPool = false; // marks the layout with UPDATE_AFTER_BIND_POOL bit
    };

    struct ReflectionOverrides {
        std::vector<BindingOverride> bindingOverrides;
        std::vector<SetOverride>     setOverrides;
    };

    // -------------------- Reflector API ------------------------------------------

    class ShaderReflector {
    public:
        // Reflect a single stage from SPIR-V words.
        // Requires HVK_USE_SPIRV_REFLECT; otherwise throws.
        static ModuleReflection reflect(const uint32_t* spirv, size_t wordCount,
            VkShaderStageFlagBits stage);

        // Merge N stage reflections (VS/FS/GS/MS/TS/CS) into set layouts and push ranges.
        // Applies overrides (flags & pool type) where specified.
        static void buildSetLayouts(const Device& device,
            const std::vector<ModuleReflection>& stages,
            const ReflectionOverrides& overrides,
            std::vector<DescriptorSetLayout>& outLayouts,
            std::vector<uint32_t>& outSetIndices /* parallel to outLayouts */);

        // Merge push constant blocks across stages (union by offset/size).
        static std::vector<VkPushConstantRange> mergePushConstants(const std::vector<ModuleReflection>& stages);

        // Convenience: make a pipeline layout from stage reflections + overrides via your cache.
        static VkPipelineLayout makePipelineLayout(PipelineLayoutCache& cache,
            const std::vector<ModuleReflection>& stages,
            const ReflectionOverrides& overrides);

        // Vertex input helper:
        // Use reflected (location, format) and patch (binding/offset/stride).
        // Returns filled VertexInputDesc (for GraphicsPipelineDesc).
        static VertexInputDesc makeVertexInput(const ModuleReflection& vsRefl,
            const std::unordered_map<uint32_t /*location*/, uint32_t /*offset*/>& locationToOffset,
            uint32_t binding,
            uint32_t stride,
            VkVertexInputRate rate = VK_VERTEX_INPUT_RATE_VERTEX);
    };

} // namespace hvk

#endif // HVK_SHADER_REFLECT_H
