#include <hvk/resources/loaders/hvk_gltf_loader.h>
#include <hvk/gfx/hvk_utils.hpp>

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <glm/gtc/type_ptr.hpp>      // make_vec3, make_vec4, make_mat4, make_quat
#include <glm/gtc/quaternion.hpp>    // quat, mat4_cast
#include <iostream>
#include <unordered_map>
#include <algorithm>                 // std::transform (for lowercase conversion)

namespace hvk {

namespace {

// Helper: Get accessor data as typed pointer
template<typename T>
const T* getAccessorData(const tinygltf::Model& model, int accessorIndex) {
    if (accessorIndex < 0) return nullptr;
    const auto& accessor = model.accessors[accessorIndex];
    const auto& bufferView = model.bufferViews[accessor.bufferView];
    const auto& buffer = model.buffers[bufferView.buffer];
    return reinterpret_cast<const T*>(&buffer.data[bufferView.byteOffset + accessor.byteOffset]);
}

// Helper: Get accessor count
size_t getAccessorCount(const tinygltf::Model& model, int accessorIndex) {
    if (accessorIndex < 0) return 0;
    return model.accessors[accessorIndex].count;
}

// Helper: Load texture from tinygltf::Image
Texture loadTextureFromImage(
    const Device& device,
    StagingUploader& uploader,
    SamplerCache& samplerCache,
    const tinygltf::Image& image,
    bool generateMips,
    bool flipY,
    bool useSRGB,  // true for color/albedo, false for data (normal, metallic-roughness, etc.)
    const std::string& name
) {
    // tinygltf already loaded the image data into image.image (std::vector<unsigned char>)
    if (image.image.empty()) {
        throw std::runtime_error("GLTF texture has no image data: " + name);
    }

    uploader.beginFrame(0);
    Texture tex = Texture::createFromMemory(
        device, uploader, samplerCache,
        image.image.data(),
        {static_cast<uint32_t>(image.width), static_cast<uint32_t>(image.height)},
        useSRGB ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM,
        generateMips,
        name
    );
    uploader.submit();
    uploader.waitCurrent();

    return tex;
}

// Helper: Determine if a texture should be SRGB based on its usage in materials
std::vector<bool> determineTextureSRGBUsage(const tinygltf::Model& gltf) {
    std::vector<bool> useSRGB(gltf.textures.size(), false);
    std::vector<std::string> usageNames(gltf.textures.size(), "");

    // Scan all materials to see how textures are used
    for (const auto& mat : gltf.materials) {
        // Albedo/base color textures should be SRGB
        int baseColorIdx = mat.pbrMetallicRoughness.baseColorTexture.index;
        if (baseColorIdx >= 0 && baseColorIdx < static_cast<int>(useSRGB.size())) {
            useSRGB[baseColorIdx] = true;
            if (usageNames[baseColorIdx].empty()) usageNames[baseColorIdx] = "baseColor";
        }

        // Emissive textures should be SRGB
        int emissiveIdx = mat.emissiveTexture.index;
        if (emissiveIdx >= 0 && emissiveIdx < static_cast<int>(useSRGB.size())) {
            useSRGB[emissiveIdx] = true;
            if (usageNames[emissiveIdx].empty()) usageNames[emissiveIdx] = "emissive";
        }

        // Track other texture types (for debugging)
        int normalIdx = mat.normalTexture.index;
        if (normalIdx >= 0 && normalIdx < static_cast<int>(useSRGB.size()) && usageNames[normalIdx].empty()) {
            usageNames[normalIdx] = "normal";
        }

        int mrIdx = mat.pbrMetallicRoughness.metallicRoughnessTexture.index;
        if (mrIdx >= 0 && mrIdx < static_cast<int>(useSRGB.size()) && usageNames[mrIdx].empty()) {
            usageNames[mrIdx] = "metallicRoughness";
        }

        int occlusionIdx = mat.occlusionTexture.index;
        if (occlusionIdx >= 0 && occlusionIdx < static_cast<int>(useSRGB.size()) && usageNames[occlusionIdx].empty()) {
            usageNames[occlusionIdx] = "occlusion";
        }
    }

    // Debug output
    std::cout << "  Texture format determination:\n";
    for (size_t i = 0; i < useSRGB.size(); ++i) {
        std::cout << "    Texture " << i << ": " << usageNames[i]
                  << " -> " << (useSRGB[i] ? "SRGB" : "LINEAR") << "\n";
    }

    return useSRGB;
}

// Helper: Load all textures
void loadTextures(
    Model& model,
    const tinygltf::Model& gltf,
    const Device& device,
    StagingUploader& uploader,
    SamplerCache& samplerCache,
    const GltfLoaderOptions& options
) {
    // Determine which textures should be SRGB based on material usage
    auto textureSRGBFlags = determineTextureSRGBUsage(gltf);

    for (size_t i = 0; i < gltf.textures.size(); ++i) {
        const auto& gltfTex = gltf.textures[i];
        if (gltfTex.source < 0 || gltfTex.source >= static_cast<int>(gltf.images.size())) {
            std::cerr << "GLTF texture " << i << " has invalid image index\n";
            continue;
        }

        const auto& image = gltf.images[gltfTex.source];
        std::string texName = image.name.empty() ? ("tex_" + std::to_string(i)) : image.name;
        bool useSRGB = textureSRGBFlags[i] && !options.forceLinearTextures;

        try {
            Texture tex = loadTextureFromImage(
                device, uploader, samplerCache, image,
                options.generateMipmaps, options.flipTextureY, useSRGB, texName
            );
            model.addTexture(std::move(tex));

            if (options.verbose) {
                std::cout << "Loaded texture: " << texName << " (" << image.width << "x" << image.height
                          << ", " << (useSRGB ? "SRGB" : "LINEAR") << ")\n";
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to load texture " << texName << ": " << e.what() << "\n";
            // Add a placeholder? For now, skip
        }
    }
}

// Helper: Load materials
void loadMaterials(
    Model& model,
    const tinygltf::Model& gltf,
    const Device& device,
    DescriptorAllocator& allocator,
    const DescriptorSetLayout& layout,
    const GltfLoaderOptions& options
) {
    for (size_t i = 0; i < gltf.materials.size(); ++i) {
        const auto& gltfMat = gltf.materials[i];

        MaterialParams params;
        params.baseColorFactor = glm::make_vec4(gltfMat.pbrMetallicRoughness.baseColorFactor.data());
        params.metallicFactor = static_cast<float>(gltfMat.pbrMetallicRoughness.metallicFactor);
        params.roughnessFactor = static_cast<float>(gltfMat.pbrMetallicRoughness.roughnessFactor);
        params.emissiveFactor = glm::make_vec3(gltfMat.emissiveFactor.data());

        // Alpha mode
        if (gltfMat.alphaMode == "OPAQUE") {
            params.alphaMode = AlphaMode::Opaque;
        } else if (gltfMat.alphaMode == "MASK") {
            params.alphaMode = AlphaMode::Mask;
            params.alphaCutoff = static_cast<float>(gltfMat.alphaCutoff);
        } else if (gltfMat.alphaMode == "BLEND") {
            params.alphaMode = AlphaMode::Blend;
        }

        // Get texture pointers (if loaded)
        Texture* albedo = nullptr;
        Texture* normal = nullptr;
        Texture* mr = nullptr;
        Texture* emissive = nullptr;
        Texture* occlusion = nullptr;

        if (options.loadTextures) {
            int baseColorTexIndex = gltfMat.pbrMetallicRoughness.baseColorTexture.index;
            if (baseColorTexIndex >= 0 && baseColorTexIndex < static_cast<int>(model.textureCount())) {
                albedo = model.texture(baseColorTexIndex);
            }

            int normalTexIndex = gltfMat.normalTexture.index;
            if (normalTexIndex >= 0 && normalTexIndex < static_cast<int>(model.textureCount())) {
                normal = model.texture(normalTexIndex);
            }

            int mrTexIndex = gltfMat.pbrMetallicRoughness.metallicRoughnessTexture.index;
            if (mrTexIndex >= 0 && mrTexIndex < static_cast<int>(model.textureCount())) {
                mr = model.texture(mrTexIndex);
            }

            int emissiveTexIndex = gltfMat.emissiveTexture.index;
            if (emissiveTexIndex >= 0 && emissiveTexIndex < static_cast<int>(model.textureCount())) {
                emissive = model.texture(emissiveTexIndex);
            }

            int occlusionTexIndex = gltfMat.occlusionTexture.index;
            if (occlusionTexIndex >= 0 && occlusionTexIndex < static_cast<int>(model.textureCount())) {
                occlusion = model.texture(occlusionTexIndex);
            }

            // Debug logging (verbose mode)
            if (options.verbose) {
                std::cout << "  Material '" << gltfMat.name << "' properties:\n";
                std::cout << "    Texture indices:\n";
                std::cout << "      baseColorTex: " << baseColorTexIndex << " (ptr: " << (albedo ? "valid" : "null") << ")\n";
                std::cout << "      normalTex: " << normalTexIndex << " (ptr: " << (normal ? "valid" : "null") << ")\n";
                std::cout << "      metallicRoughnessTex: " << mrTexIndex << " (ptr: " << (mr ? "valid" : "null") << ")\n";
                std::cout << "      emissiveTex: " << emissiveTexIndex << " (ptr: " << (emissive ? "valid" : "null") << ")\n";
                std::cout << "      occlusionTex: " << occlusionTexIndex << " (ptr: " << (occlusion ? "valid" : "null") << ")\n";
                std::cout << "    Material factors:\n";
                std::cout << "      baseColorFactor: (" << params.baseColorFactor.r << ", "
                          << params.baseColorFactor.g << ", " << params.baseColorFactor.b << ", " << params.baseColorFactor.a << ")\n";
                std::cout << "      emissiveFactor: (" << params.emissiveFactor.r << ", "
                          << params.emissiveFactor.g << ", " << params.emissiveFactor.b << ")\n";
                std::cout << "    Alpha properties:\n";
                std::cout << "      alphaMode: " << gltfMat.alphaMode << " (";
                if (params.alphaMode == AlphaMode::Opaque) std::cout << "OPAQUE";
                else if (params.alphaMode == AlphaMode::Mask) std::cout << "MASK";
                else if (params.alphaMode == AlphaMode::Blend) std::cout << "BLEND";
                std::cout << ")\n";
                std::cout << "      alphaCutoff: " << params.alphaCutoff << "\n";
                std::cout << "      doubleSided: " << (gltfMat.doubleSided ? "true" : "false") << "\n";
            }

            // WORKAROUND: Some exporters incorrectly use emissive texture slot for albedo/baseColor
            // Apply this workaround ONLY if ALL conditions are met (conservative approach):
            bool applyWorkaround = false;
            if (baseColorTexIndex == -1 &&  // 1. No baseColor texture
                emissiveTexIndex >= 0 &&     // 2. Has emissive texture
                albedo == nullptr &&         // 3. Albedo pointer is null
                emissive != nullptr) {       // 4. Emissive pointer is valid

                // 5. Check if baseColorFactor is near-black (< 0.01)
                float baseColorLength = glm::length(glm::vec3(params.baseColorFactor));

                // 6. Check if emissiveFactor is near-white (within 0.1 of (1,1,1))
                glm::vec3 emissiveVec(params.emissiveFactor);
                float emissiveWhiteness = glm::length(emissiveVec - glm::vec3(1.0f, 1.0f, 1.0f));

                // 7. Material name doesn't suggest it's actually emissive
                std::string matNameLower = gltfMat.name;
                std::transform(matNameLower.begin(), matNameLower.end(), matNameLower.begin(), ::tolower);
                bool hasEmissiveName = (matNameLower.find("emissive") != std::string::npos ||
                                        matNameLower.find("glow") != std::string::npos ||
                                        matNameLower.find("emit") != std::string::npos);

                applyWorkaround = (baseColorLength < 0.01f) &&
                                  (emissiveWhiteness < 0.1f) &&
                                  !hasEmissiveName;

                if (options.verbose) {
                    std::cout << "    Workaround check:\n";
                    std::cout << "      baseColorLength: " << baseColorLength << " (< 0.01: " << (baseColorLength < 0.01f ? "yes" : "no") << ")\n";
                    std::cout << "      emissiveWhiteness: " << emissiveWhiteness << " (< 0.1: " << (emissiveWhiteness < 0.1f ? "yes" : "no") << ")\n";
                    std::cout << "      hasEmissiveName: " << (hasEmissiveName ? "yes" : "no") << "\n";
                    std::cout << "      applyWorkaround: " << (applyWorkaround ? "YES" : "NO") << "\n";
                }
            }

            if (applyWorkaround) {
                std::cout << "  [WORKAROUND] Material '" << gltfMat.name
                          << "' appears to use emissive texture as albedo (swapping slots)\n";
                albedo = emissive;
                emissive = nullptr;
            }
        }

        // Create material
        Material mat;
        mat.init(device, allocator, layout, params,
                 albedo, normal, mr, emissive, occlusion,
                 model.defaultWhiteTexture(),
                 model.defaultNormalTexture(),
                 model.defaultMetallicRoughnessTexture());
        mat.setName(gltfMat.name.empty() ? ("mat_" + std::to_string(i)) : gltfMat.name);

        model.addMaterial(std::move(mat));

        if (options.verbose) {
            std::cout << "Loaded material: " << gltfMat.name << "\n";
        }
    }
}

// Helper: Load meshes
void loadMeshes(
    Model& model,
    const tinygltf::Model& gltf,
    const Device& device,
    StagingUploader& uploader,
    const GltfLoaderOptions& options
) {
    for (size_t meshIdx = 0; meshIdx < gltf.meshes.size(); ++meshIdx) {
        const auto& gltfMesh = gltf.meshes[meshIdx];

        for (size_t primIdx = 0; primIdx < gltfMesh.primitives.size(); ++primIdx) {
            const auto& prim = gltfMesh.primitives[primIdx];

            // Only support TRIANGLES for now
            if (prim.mode != TINYGLTF_MODE_TRIANGLES) {
                std::cerr << "Skipping non-triangle primitive in mesh " << gltfMesh.name << "\n";
                continue;
            }

            std::vector<Vertex> vertices;
            std::vector<uint32_t> indices;

            // --- Load indices ---
            if (prim.indices >= 0) {
                const auto& accessor = gltf.accessors[prim.indices];
                size_t indexCount = accessor.count;
                indices.resize(indexCount);

                if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
                    const uint16_t* buf = getAccessorData<uint16_t>(gltf, prim.indices);
                    for (size_t i = 0; i < indexCount; ++i) {
                        indices[i] = buf[i];
                    }
                } else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                    const uint32_t* buf = getAccessorData<uint32_t>(gltf, prim.indices);
                    std::memcpy(indices.data(), buf, indexCount * sizeof(uint32_t));
                } else {
                    std::cerr << "Unsupported index component type\n";
                    continue;
                }
            }

            // --- Load vertices ---
            auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end()) {
                std::cerr << "Mesh primitive missing POSITION attribute\n";
                continue;
            }

            size_t vertexCount = getAccessorCount(gltf, posIt->second);
            vertices.resize(vertexCount);

            // Positions
            const glm::vec3* positions = getAccessorData<glm::vec3>(gltf, posIt->second);
            for (size_t i = 0; i < vertexCount; ++i) {
                vertices[i].position = positions[i];
            }

            // Normals
            auto normIt = prim.attributes.find("NORMAL");
            if (normIt != prim.attributes.end()) {
                const glm::vec3* normals = getAccessorData<glm::vec3>(gltf, normIt->second);
                for (size_t i = 0; i < vertexCount; ++i) {
                    vertices[i].normal = normals[i];
                }
            }

            // UVs (TEXCOORD_0)
            auto uvIt = prim.attributes.find("TEXCOORD_0");
            if (uvIt != prim.attributes.end()) {
                const glm::vec2* uvs = getAccessorData<glm::vec2>(gltf, uvIt->second);
                for (size_t i = 0; i < vertexCount; ++i) {
                    vertices[i].uv = uvs[i];
                }
            }

            // Colors (COLOR_0)
            auto colorIt = prim.attributes.find("COLOR_0");
            if (colorIt != prim.attributes.end()) {
                const auto& accessor = gltf.accessors[colorIt->second];
                if (accessor.type == TINYGLTF_TYPE_VEC4) {
                    const glm::vec4* colors = getAccessorData<glm::vec4>(gltf, colorIt->second);
                    for (size_t i = 0; i < vertexCount; ++i) {
                        vertices[i].color = colors[i];
                    }
                }
            }

            // Tangents (TANGENT)
            auto tangentIt = prim.attributes.find("TANGENT");
            if (tangentIt != prim.attributes.end()) {
                const glm::vec4* tangents = getAccessorData<glm::vec4>(gltf, tangentIt->second);
                for (size_t i = 0; i < vertexCount; ++i) {
                    vertices[i].tangent = tangents[i];
                }
            }

            // Create mesh
            uploader.beginFrame(0);
            Mesh mesh;
            std::string meshName = gltfMesh.name + "_prim" + std::to_string(primIdx);
            mesh.create(device, uploader, vertices, indices, nullptr, meshName);
            uploader.submit();
            uploader.waitCurrent();

            // Assign material
            if (prim.material >= 0 && prim.material < static_cast<int>(model.materialCount())) {
                mesh.setMaterial(model.material(prim.material));
            }

            model.addMesh(std::move(mesh));

            if (options.verbose) {
                std::cout << "Loaded mesh: " << meshName << " (" << vertexCount << " verts, "
                          << indices.size() << " indices)\n";
            }
        }
    }
}

// Helper: Load nodes
void loadNodes(Model& model, const tinygltf::Model& gltf, const GltfLoaderOptions& options) {
    // First pass: create all nodes
    for (size_t i = 0; i < gltf.nodes.size(); ++i) {
        const auto& gltfNode = gltf.nodes[i];

        Node node;
        node.name = gltfNode.name.empty() ? ("node_" + std::to_string(i)) : gltfNode.name;

        // Compute local transform
        if (!gltfNode.matrix.empty()) {
            // Matrix provided directly
            node.localTransform = glm::make_mat4(gltfNode.matrix.data());
        } else {
            // Compute from TRS
            glm::mat4 translation(1.0f);
            glm::mat4 rotation(1.0f);
            glm::mat4 scale(1.0f);

            if (!gltfNode.translation.empty()) {
                glm::vec3 t(gltfNode.translation[0], gltfNode.translation[1], gltfNode.translation[2]);
                translation = glm::translate(glm::mat4(1.0f), t);
            }
            if (!gltfNode.rotation.empty()) {
                glm::quat q(
                    static_cast<float>(gltfNode.rotation[3]),  // w first in glm::quat constructor
                    static_cast<float>(gltfNode.rotation[0]),  // x
                    static_cast<float>(gltfNode.rotation[1]),  // y
                    static_cast<float>(gltfNode.rotation[2])   // z
                );
                rotation = glm::mat4_cast(q);
            }
            if (!gltfNode.scale.empty()) {
                glm::vec3 s(gltfNode.scale[0], gltfNode.scale[1], gltfNode.scale[2]);
                scale = glm::scale(glm::mat4(1.0f), s);
            }

            node.localTransform = translation * rotation * scale;
        }

        // Mesh reference
        if (gltfNode.mesh >= 0) {
            node.meshIndex = gltfNode.mesh;
        }

        model.addNode(node);
    }

    // Second pass: setup parent-child relationships
    for (size_t i = 0; i < gltf.nodes.size(); ++i) {
        const auto& gltfNode = gltf.nodes[i];
        Node* node = model.node(i);

        for (int childIdx : gltfNode.children) {
            if (childIdx >= 0 && childIdx < static_cast<int>(model.nodeCount())) {
                node->children.push_back(childIdx);
                model.node(childIdx)->parentIndex = static_cast<int32_t>(i);
            }
        }
    }

    // Set root node (first node in default scene, or first node if no scene)
    if (!gltf.scenes.empty()) {
        int defaultScene = gltf.defaultScene >= 0 ? gltf.defaultScene : 0;
        if (defaultScene < static_cast<int>(gltf.scenes.size())) {
            const auto& scene = gltf.scenes[defaultScene];
            if (!scene.nodes.empty()) {
                model.setRootNode(scene.nodes[0]);
            }
        }
    } else if (model.nodeCount() > 0) {
        model.setRootNode(0);
    }

    if (options.verbose) {
        std::cout << "Loaded " << model.nodeCount() << " nodes\n";
    }
}

} // anonymous namespace

Model GltfLoader::loadFromFile(
    const Device& device,
    StagingUploader& uploader,
    DescriptorAllocator& descriptorAllocator,
    const DescriptorSetLayout& materialLayout,
    SamplerCache& samplerCache,
    const std::string& filepath,
    const GltfLoaderOptions& options
) {
    if (options.verbose) {
        std::cout << "Loading GLTF: " << filepath << "\n";
    }

    // Load GLTF file using tinygltf
    tinygltf::Model gltf;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    bool success = false;
    if (filepath.ends_with(".glb")) {
        success = loader.LoadBinaryFromFile(&gltf, &err, &warn, filepath);
    } else {
        success = loader.LoadASCIIFromFile(&gltf, &err, &warn, filepath);
    }

    if (!warn.empty()) {
        std::cerr << "GLTF Warning: " << warn << "\n";
    }
    if (!err.empty()) {
        std::cerr << "GLTF Error: " << err << "\n";
    }
    if (!success) {
        throw std::runtime_error("Failed to load GLTF file: " + filepath);
    }

    // Create model
    Model model;
    model.setName(filepath);

    // Create default textures
    model.createDefaultTextures(device, uploader, samplerCache);

    // Reserve space for textures to prevent vector reallocation (which would invalidate pointers)
    if (options.loadTextures) {
        model.reserveTextures(gltf.textures.size());
    }

    // Load in order: textures -> materials -> meshes -> nodes
    if (options.loadTextures) {
        loadTextures(model, gltf, device, uploader, samplerCache, options);
    }

    if (options.loadMaterials) {
        loadMaterials(model, gltf, device, descriptorAllocator, materialLayout, options);
    }

    loadMeshes(model, gltf, device, uploader, options);
    loadNodes(model, gltf, options);

    // Update transforms
    model.updateTransforms();

    if (options.verbose) {
        std::cout << "GLTF loaded successfully:\n";
        std::cout << "  Textures: " << model.textureCount() << "\n";
        std::cout << "  Materials: " << model.materialCount() << "\n";
        std::cout << "  Meshes: " << model.meshCount() << "\n";
        std::cout << "  Nodes: " << model.nodeCount() << "\n";
    }

    return model;
}

} // namespace hvk
