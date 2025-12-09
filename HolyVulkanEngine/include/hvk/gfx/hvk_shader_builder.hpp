/**
 * @file hvk_shader_builder.hpp
 * @brief Fluent API for loading and creating Vulkan shader modules
 * @author Holy Vulkan Engine
 * @date 2025
 *
 * Simplifies shader loading from SPIR-V files with automatic error handling.
 * Reduces typical shader loading boilerplate from ~30 lines to ~3 lines.
 *
 * Usage example:
 * @code
 * auto shaders = ShaderBuilder()
 *     .loadVertex(PROJECT_ROOT "/shaders/model.vert.spv")
 *     .loadFragment(PROJECT_ROOT "/shaders/model.frag.spv")
 *     .build(device);
 *
 * // Use shaders.vertex, shaders.fragment, shaders.stages
 * // Don't forget to destroy modules when done!
 * vkDestroyShaderModule(device, shaders.vertex, nullptr);
 * vkDestroyShaderModule(device, shaders.fragment, nullptr);
 * @endcode
 */

#ifndef HVK_SHADER_BUILDER_HPP
#define HVK_SHADER_BUILDER_HPP

#include <hvk/gfx/hvk_graphics_pipeline_cache.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace hvk {

// Forward declaration
class Device;

/**
 * ShaderModules - Result of ShaderBuilder::build()
 *
 * Contains created shader modules and corresponding stage descriptors
 * ready to use in pipeline creation.
 */
struct ShaderModules {
    VkShaderModule vertex = VK_NULL_HANDLE;
    VkShaderModule fragment = VK_NULL_HANDLE;
    VkShaderModule compute = VK_NULL_HANDLE;
    VkShaderModule geometry = VK_NULL_HANDLE;
    VkShaderModule tessControl = VK_NULL_HANDLE;
    VkShaderModule tessEval = VK_NULL_HANDLE;

    // Stage descriptors ready for pipeline creation
    std::vector<ShaderStageDesc> stages;

    /**
     * Check if any shader modules were created
     */
    bool hasShaders() const {
        return vertex != VK_NULL_HANDLE || fragment != VK_NULL_HANDLE ||
               compute != VK_NULL_HANDLE || geometry != VK_NULL_HANDLE ||
               tessControl != VK_NULL_HANDLE || tessEval != VK_NULL_HANDLE;
    }
};

/**
 * ShaderBuilder - Fluent API for shader loading
 *
 * Simplifies the process of loading SPIR-V shaders and creating Vulkan shader modules.
 * Handles file I/O, validation, and shader module creation.
 *
 * Design:
 * - Fluent interface (chainable methods)
 * - Automatic SPIR-V validation
 * - Clear error messages
 * - Minimal boilerplate
 *
 * Usage:
 * @code
 * // Graphics shaders
 * auto shaders = ShaderBuilder()
 *     .loadVertex("shaders/pbr.vert.spv")
 *     .loadFragment("shaders/pbr.frag.spv")
 *     .build(device);
 *
 * // Compute shader
 * auto computeShader = ShaderBuilder()
 *     .loadCompute("shaders/particles.comp.spv")
 *     .build(device);
 *
 * // With custom entry points
 * auto shaders = ShaderBuilder()
 *     .loadVertex("shaders/advanced.spv", "vertexMain")
 *     .loadFragment("shaders/advanced.spv", "fragmentMain")
 *     .build(device);
 * @endcode
 */
class ShaderBuilder {
public:
    ShaderBuilder() = default;
    ~ShaderBuilder() = default;

    /**
     * Load vertex shader from SPIR-V file
     *
     * @param path Path to .spv file
     * @param entryPoint Entry point function name (default: "main")
     * @return *this for chaining
     */
    ShaderBuilder& loadVertex(const char* path, const char* entryPoint = "main");

    /**
     * Load fragment shader from SPIR-V file
     *
     * @param path Path to .spv file
     * @param entryPoint Entry point function name (default: "main")
     * @return *this for chaining
     */
    ShaderBuilder& loadFragment(const char* path, const char* entryPoint = "main");

    /**
     * Load compute shader from SPIR-V file
     *
     * @param path Path to .spv file
     * @param entryPoint Entry point function name (default: "main")
     * @return *this for chaining
     */
    ShaderBuilder& loadCompute(const char* path, const char* entryPoint = "main");

    /**
     * Load geometry shader from SPIR-V file
     *
     * @param path Path to .spv file
     * @param entryPoint Entry point function name (default: "main")
     * @return *this for chaining
     */
    ShaderBuilder& loadGeometry(const char* path, const char* entryPoint = "main");

    /**
     * Load tessellation control shader from SPIR-V file
     *
     * @param path Path to .spv file
     * @param entryPoint Entry point function name (default: "main")
     * @return *this for chaining
     */
    ShaderBuilder& loadTessControl(const char* path, const char* entryPoint = "main");

    /**
     * Load tessellation evaluation shader from SPIR-V file
     *
     * @param path Path to .spv file
     * @param entryPoint Entry point function name (default: "main")
     * @return *this for chaining
     */
    ShaderBuilder& loadTessEval(const char* path, const char* entryPoint = "main");

    /**
     * Build shader modules and create stage descriptors
     *
     * Creates Vulkan shader modules for all loaded shaders and generates
     * ShaderStageDesc array suitable for pipeline creation.
     *
     * @param device Vulkan device wrapper
     * @return ShaderModules containing modules and stage descriptors
     * @throws std::runtime_error if shader creation fails
     */
    ShaderModules build(const Device& device);

private:
    struct ShaderInfo {
        std::string path;
        std::string entryPoint;
        VkShaderStageFlagBits stage;
    };

    std::vector<ShaderInfo> shaders_;

    /**
     * Load SPIR-V bytecode from file
     *
     * @param path Path to .spv file
     * @return SPIR-V bytecode as uint32_t array
     * @throws std::runtime_error if file can't be read or is invalid
     */
    static std::vector<uint32_t> loadSpirv(const char* path);

    /**
     * Create Vulkan shader module from SPIR-V code
     *
     * @param device Vulkan device
     * @param code SPIR-V bytecode
     * @param debugName Optional debug name for the module
     * @return Created shader module
     * @throws std::runtime_error if module creation fails
     */
    static VkShaderModule createShaderModule(
        VkDevice device,
        const std::vector<uint32_t>& code,
        const char* debugName = nullptr
    );
};

} // namespace hvk

#endif // HVK_SHADER_BUILDER_HPP
