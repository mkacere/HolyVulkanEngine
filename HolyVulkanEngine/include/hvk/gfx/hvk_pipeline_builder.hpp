/**
 * @file hvk_pipeline_builder.hpp
 * @brief Fluent API for creating Vulkan graphics pipelines
 * @author Holy Vulkan Engine
 * @date 2025
 *
 * Simplifies graphics pipeline creation with preset configurations and
 * a chainable builder pattern. Reduces typical pipeline setup from ~40 lines to ~5 lines.
 *
 * Usage example:
 * @code
 * VkPipeline opaque = PipelineBuilder()
 *     .setShaders(shaders)
 *     .setLayout(pipelineLayout)
 *     .setVertexInput(Vertex::getInputDesc())
 *     .makeOpaque()  // Preset: depth test ON, depth write ON, no blending
 *     .build(pipelineCache, renderFormats);
 *
 * VkPipeline transparent = PipelineBuilder()
 *     .setShaders(shaders)
 *     .setLayout(pipelineLayout)
 *     .makeTransparent()  // Preset: depth test ON, depth write OFF, alpha blending
 *     .build(pipelineCache, renderFormats);
 * @endcode
 */

#ifndef HVK_PIPELINE_BUILDER_HPP
#define HVK_PIPELINE_BUILDER_HPP

#include <hvk/gfx/hvk_graphics_pipeline_cache.h>
#include <hvk/gfx/hvk_shader_builder.hpp>
#include <vulkan/vulkan.h>

namespace hvk {

// Forward declarations
class GraphicsPipelineCache;

/**
 * PipelineBuilder - Fluent API for graphics pipeline creation
 *
 * Provides a chainable interface for configuring graphics pipelines with
 * sensible defaults and preset configurations for common rendering modes.
 *
 * Design:
 * - Fluent interface (all methods return *this)
 * - Preset methods for common configurations (opaque, masked, transparent)
 * - Individual state override methods for customization
 * - Integrates with GraphicsPipelineCache for automatic caching
 *
 * Preset Configurations:
 * - **makeOpaque()**: Solid geometry with depth testing and writing
 * - **makeMasked()**: Alpha-to-coverage for foliage/hair (alpha cutout)
 * - **makeTransparent()**: Alpha blending for glass/particles
 * - **makeFullscreen()**: Fullscreen effects (no depth, no vertex input)
 *
 * Usage Patterns:
 * @code
 * // Simple opaque rendering
 * auto pipeline = PipelineBuilder()
 *     .setShaders(shaders)
 *     .setLayout(layout)
 *     .setVertexInput(vertexDesc)
 *     .makeOpaque()
 *     .build(cache, formats);
 *
 * // Customize a preset
 * auto pipeline = PipelineBuilder()
 *     .setShaders(shaders)
 *     .setLayout(layout)
 *     .makeOpaque()
 *     .setCullMode(VK_CULL_MODE_NONE)  // Override cull mode
 *     .build(cache, formats);
 *
 * // Build from scratch
 * auto pipeline = PipelineBuilder()
 *     .setShaders(shaders)
 *     .setLayout(layout)
 *     .setTopology(VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
 *     .setCullMode(VK_CULL_MODE_NONE)
 *     .setDepthTest(true, false)
 *     .setBlending(false)
 *     .build(cache, formats);
 * @endcode
 */
class PipelineBuilder {
public:
    PipelineBuilder();
    ~PipelineBuilder() = default;

    // ========================================================================
    // Required Configuration
    // ========================================================================

    /**
     * Set shader stages (required)
     *
     * @param shaders ShaderModules from ShaderBuilder
     * @return *this for chaining
     */
    PipelineBuilder& setShaders(const ShaderModules& shaders);

    /**
     * Set pipeline layout (required)
     *
     * @param layout VkPipelineLayout for descriptor sets and push constants
     * @return *this for chaining
     */
    PipelineBuilder& setLayout(VkPipelineLayout layout);

    // ========================================================================
    // Vertex Input Configuration
    // ========================================================================

    /**
     * Set vertex input description
     *
     * @param vertexInput Vertex bindings and attributes
     * @return *this for chaining
     */
    PipelineBuilder& setVertexInput(const VertexInputDesc& vertexInput);

    /**
     * Use no vertex input (for fullscreen effects, compute-generated geometry, etc.)
     *
     * @return *this for chaining
     */
    PipelineBuilder& noVertexInput();

    // ========================================================================
    // Preset Configurations
    // ========================================================================

    /**
     * Configure for opaque rendering
     *
     * Settings:
     * - Depth test: ENABLED, write ENABLED, compare LESS_OR_EQUAL
     * - Blending: DISABLED
     * - Cull mode: BACK
     * - Topology: TRIANGLE_LIST
     *
     * @return *this for chaining
     */
    PipelineBuilder& makeOpaque();

    /**
     * Configure for alpha-masked rendering (alpha-to-coverage)
     *
     * Settings:
     * - Depth test: ENABLED, write ENABLED, compare LESS_OR_EQUAL
     * - Blending: DISABLED
     * - Alpha to coverage: ENABLED
     * - Cull mode: NONE (typical for foliage)
     * - Topology: TRIANGLE_LIST
     *
     * @return *this for chaining
     */
    PipelineBuilder& makeMasked();

    /**
     * Configure for transparent rendering (alpha blending)
     *
     * Settings:
     * - Depth test: ENABLED, write DISABLED (no depth writes)
     * - Blending: ENABLED (src_alpha, one_minus_src_alpha)
     * - Cull mode: NONE
     * - Topology: TRIANGLE_LIST
     *
     * @return *this for chaining
     */
    PipelineBuilder& makeTransparent();

    /**
     * Configure for additive blending (particles, lights)
     *
     * Settings:
     * - Depth test: ENABLED, write DISABLED
     * - Blending: ENABLED (src_alpha, one - additive)
     * - Cull mode: NONE
     * - Topology: TRIANGLE_LIST
     *
     * @return *this for chaining
     */
    PipelineBuilder& makeAdditive();

    /**
     * Configure for fullscreen post-processing
     *
     * Settings:
     * - No vertex input
     * - Depth test: DISABLED
     * - Blending: DISABLED
     * - Cull mode: NONE
     * - Topology: TRIANGLE_LIST
     *
     * @return *this for chaining
     */
    PipelineBuilder& makeFullscreen();

    // ========================================================================
    // Individual State Overrides
    // ========================================================================

    /**
     * Set primitive topology
     *
     * @param topology Primitive topology (TRIANGLE_LIST, LINE_LIST, etc.)
     * @return *this for chaining
     */
    PipelineBuilder& setTopology(VkPrimitiveTopology topology);

    /**
     * Set polygon mode
     *
     * @param mode Polygon mode (FILL, LINE, POINT)
     * @return *this for chaining
     */
    PipelineBuilder& setPolygonMode(VkPolygonMode mode);

    /**
     * Set cull mode
     *
     * @param mode Cull mode (NONE, FRONT, BACK, FRONT_AND_BACK)
     * @return *this for chaining
     */
    PipelineBuilder& setCullMode(VkCullModeFlags mode);

    /**
     * Set front face winding
     *
     * @param frontFace Front face winding (CLOCKWISE, COUNTER_CLOCKWISE)
     * @return *this for chaining
     */
    PipelineBuilder& setFrontFace(VkFrontFace frontFace);

    /**
     * Set depth testing configuration
     *
     * @param testEnable Enable depth testing
     * @param writeEnable Enable depth writes
     * @param compareOp Depth comparison function (default: LESS_OR_EQUAL)
     * @return *this for chaining
     */
    PipelineBuilder& setDepthTest(
        bool testEnable,
        bool writeEnable,
        VkCompareOp compareOp = VK_COMPARE_OP_LESS_OR_EQUAL
    );

    /**
     * Set blending configuration
     *
     * @param enable Enable blending
     * @param srcColor Source color blend factor
     * @param dstColor Destination color blend factor
     * @param srcAlpha Source alpha blend factor (default: same as srcColor)
     * @param dstAlpha Destination alpha blend factor (default: same as dstColor)
     * @return *this for chaining
     */
    PipelineBuilder& setBlending(
        bool enable,
        VkBlendFactor srcColor = VK_BLEND_FACTOR_ONE,
        VkBlendFactor dstColor = VK_BLEND_FACTOR_ZERO,
        VkBlendFactor srcAlpha = VK_BLEND_FACTOR_ONE,
        VkBlendFactor dstAlpha = VK_BLEND_FACTOR_ZERO
    );

    /**
     * Enable alpha-to-coverage (for alpha masking/foliage)
     *
     * @param enable Enable alpha-to-coverage
     * @return *this for chaining
     */
    PipelineBuilder& setAlphaToCoverage(bool enable);

    /**
     * Set MSAA sample count
     *
     * @param samples Sample count (1, 2, 4, 8, etc.)
     * @return *this for chaining
     */
    PipelineBuilder& setSampleCount(VkSampleCountFlagBits samples);

    // ========================================================================
    // Build
    // ========================================================================

    /**
     * Build the graphics pipeline
     *
     * Creates the pipeline using the configured state and caches it
     * via the provided GraphicsPipelineCache.
     *
     * @param cache Pipeline cache for automatic caching
     * @param formats Render target formats (color + depth)
     * @return Created VkPipeline handle (cached, don't destroy manually)
     * @throws std::runtime_error if required configuration is missing
     */
    VkPipeline build(GraphicsPipelineCache& cache, const RenderFormats& formats);

private:
    // Pipeline description built from configuration
    GraphicsPipelineDesc desc_;

    // Track what's been configured
    bool hasShaders_ = false;
    bool hasLayout_ = false;
};

} // namespace hvk

#endif // HVK_PIPELINE_BUILDER_HPP
