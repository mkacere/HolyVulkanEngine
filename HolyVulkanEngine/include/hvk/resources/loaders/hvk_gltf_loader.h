#ifndef HVK_GLTF_LOADER_H
#define HVK_GLTF_LOADER_H

#include <hvk/gfx/hvk_device.h>
#include <hvk/gfx/hvk_descriptors.h>
#include <hvk/gfx/hvk_staging_uploader.h>
#include <hvk/gfx/hvk_sampler_cache.h>
#include <hvk/resources/hvk_model.h>

#include <string>
#include <vector>

namespace hvk {

/**
 * GltfLoaderOptions - Configuration for GLTF loading
 */
struct GltfLoaderOptions {
    bool generateMipmaps = true;        // Auto-generate mipmaps for textures
    bool loadMaterials = true;          // Load materials (if false, uses default material)
    bool loadTextures = true;           // Load textures (if false, uses default textures)
    bool flipTextureY = false;          // Flip texture Y coordinate on load
    bool forceLinearTextures = false;   // Force all textures to linear (no sRGB)
    bool verbose = false;               // Print loading progress to console

    GltfLoaderOptions() = default;
};

/**
 * GltfLoader - Loads GLTF/GLB files into Model
 *
 * Supports:
 * - GLTF 2.0 format (.gltf JSON + external files, .glb binary)
 * - PBR materials (metallic-roughness workflow)
 * - Textures (embedded or external)
 * - Scene graph with node hierarchy and transforms
 * - Multiple meshes and materials
 * - Vertex colors, normals, UVs, tangents
 *
 * Usage:
 *   Model model = GltfLoader::loadFromFile(
 *       device, uploader, allocator, layout, samplerCache,
 *       "path/to/model.gltf"
 *   );
 *   model.updateTransforms();
 *   model.draw(cmd, pipelineLayout, globalDescSet);
 */
class GltfLoader {
public:
    /**
     * Load a GLTF/GLB file
     *
     * @param device Device reference
     * @param uploader Staging uploader for GPU uploads
     * @param descriptorAllocator Descriptor allocator for materials
     * @param materialLayout Descriptor set layout for materials (Set 1)
     * @param samplerCache Sampler cache for textures
     * @param filepath Path to .gltf or .glb file
     * @param options Loading options
     * @return Loaded model
     */
    static Model loadFromFile(
        const Device& device,
        StagingUploader& uploader,
        DescriptorAllocator& descriptorAllocator,
        const DescriptorSetLayout& materialLayout,
        SamplerCache& samplerCache,
        const std::string& filepath,
        const GltfLoaderOptions& options = GltfLoaderOptions()
    );

private:
    // Internal helper methods (implementation details)
    // These are declared here but implemented in the .cpp file
};

} // namespace hvk

#endif // HVK_GLTF_LOADER_H
