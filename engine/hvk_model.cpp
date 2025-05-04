#include "hvk_model.h"
#include "hvk_utils.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#define TINYGLTF_IMPLEMENTATION
#include <tiny_gltf.h>

#include <unordered_map>
#include <iostream>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include "hvk_utils.hpp"

#include <functional>
namespace std {
	template<>
	struct hash<hvk::HvkModel::Vertex> {
		size_t operator()(hvk::HvkModel::Vertex const& v) const noexcept {
			size_t seed = 0;
			hvk::hashCombine(seed, v.position, v.color, v.normal, v.uv);
			return seed;
		}
	};
}

namespace hvk {

	void HvkModel::Builder::loadModel(const std::string& filepath) {
		// 1) Load into our member-model, not a local
		tinygltf::TinyGLTF loader;
		std::string err, warn;
		bool ok = filepath.rfind(".glb") != std::string::npos
			? loader.LoadBinaryFromFile(&model, &err, &warn, filepath)
			: loader.LoadASCIIFromFile(&model, &err, &warn, filepath);
		if (!warn.empty()) std::cerr << "tinygltf warning: " << warn << "\n";
		if (!err.empty())  throw std::runtime_error("tinygltf error: " + err);
		if (!ok)          throw std::runtime_error("Failed to load glTF: " + filepath);

		auto& gltf = this->model;  // alias for brevity

		// 2) Pull out PBR material info (we just take the first material)
		if (!gltf.materials.empty()) {
			auto& pbr = gltf.materials[0].pbrMetallicRoughness;
			baseColorFactor = glm::vec4(
				float(pbr.baseColorFactor[0]),
				float(pbr.baseColorFactor[1]),
				float(pbr.baseColorFactor[2]),
				float(pbr.baseColorFactor[3])
			);
			if (pbr.baseColorTexture.index >= 0) {
				auto& tex = gltf.textures[pbr.baseColorTexture.index];
				baseColorImage = &gltf.images[tex.source];
				hasBaseColor = true;
				texCoordBase = glm::vec2(pbr.baseColorTexture.texCoord);
			}
			if (pbr.metallicRoughnessTexture.index >= 0) {
				auto& tex = gltf.textures[pbr.metallicRoughnessTexture.index];
				mrImage = &gltf.images[tex.source];
				hasMR = true;
				texCoordMR = glm::vec2(pbr.metallicRoughnessTexture.texCoord);
			}
		}
		// 3) Optional normal/emissive
		if (!gltf.materials.empty()) {
			auto& mat = gltf.materials[0];
			if (mat.normalTexture.index >= 0) {
				auto& tex = gltf.textures[mat.normalTexture.index];
				normalImage = &gltf.images[tex.source];
				hasNormalMap = true;
				texCoordNM = glm::vec2(mat.normalTexture.texCoord);
			}
			if (mat.emissiveTexture.index >= 0) {
				auto& tex = gltf.textures[mat.emissiveTexture.index];
				emissiveImage = &gltf.images[tex.source];
				hasEmissive = true;
				texCoordEM = glm::vec2(mat.emissiveTexture.texCoord);
			}
		}

		// 4) Extract geometry
		vertices.clear();
		indices.clear();
		std::unordered_map<Vertex, uint32_t> uniqueVertices;

		// Helper to grab a raw pointer into any buffer
		auto getDataPtr = [&](const tinygltf::Accessor& acc) {
			const auto& view = gltf.bufferViews[acc.bufferView];
			const auto& buffer = gltf.buffers[view.buffer];
			return buffer.data.data() + view.byteOffset + acc.byteOffset;
			};

		for (auto const& mesh : gltf.meshes) {
			for (auto const& prim : mesh.primitives) {
				// POSITION is required
				const auto& posAcc = gltf.accessors.at(prim.attributes.at("POSITION"));
				const float* positions = reinterpret_cast<const float*>(getDataPtr(posAcc));
				size_t vertCount = posAcc.count;

				// Optional attributes
				const tinygltf::Accessor* normAcc = prim.attributes.count("NORMAL")
					? &gltf.accessors.at(prim.attributes.at("NORMAL")) : nullptr;
				const tinygltf::Accessor* uvAcc = prim.attributes.count("TEXCOORD_0")
					? &gltf.accessors.at(prim.attributes.at("TEXCOORD_0")) : nullptr;
				const tinygltf::Accessor* colAcc = prim.attributes.count("COLOR_0")
					? &gltf.accessors.at(prim.attributes.at("COLOR_0")) : nullptr;

				// Build one Vertex from index i
				auto buildVertex = [&](uint32_t i) {
					Vertex v{};
					v.position = { positions[3 * i + 0], positions[3 * i + 1], positions[3 * i + 2] };
					if (normAcc) {
						const float* n = reinterpret_cast<const float*>(getDataPtr(*normAcc));
						v.normal = { n[3 * i + 0], n[3 * i + 1], n[3 * i + 2] };
					}
					if (uvAcc) {
						const float* u = reinterpret_cast<const float*>(getDataPtr(*uvAcc));
						v.uv = { u[2 * i + 0], u[2 * i + 1] };
					}
					if (colAcc) {
						const float* c = reinterpret_cast<const float*>(getDataPtr(*colAcc));
						v.color = { c[3 * i + 0], c[3 * i + 1], c[3 * i + 2] };
					}
					else {
						v.color = { 1.f,1.f,1.f };
					}
					return v;
					};

				// If this primitive has an index buffer
				if (prim.indices > -1) {
					const auto& idxAcc = gltf.accessors.at(prim.indices);
					const void* idxData = getDataPtr(idxAcc);
					for (size_t k = 0; k < idxAcc.count; k++) {
						uint32_t gltfIndex;
						switch (idxAcc.componentType) {
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
							gltfIndex = reinterpret_cast<const uint16_t*>(idxData)[k];
							break;
						case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
							gltfIndex = reinterpret_cast<const uint32_t*>(idxData)[k];
							break;
						default:
							throw std::runtime_error("Unsupported index component type");
						}
						Vertex vert = buildVertex(gltfIndex);
						auto it = uniqueVertices.find(vert);
						if (it == uniqueVertices.end()) {
							uint32_t newIndex = uint32_t(vertices.size());
							uniqueVertices[vert] = newIndex;
							vertices.push_back(vert);
							indices.push_back(newIndex);
						}
						else {
							indices.push_back(it->second);
						}
					}
				}
				else {
					// No index, just one‐to‐one
					for (uint32_t i = 0; i < vertCount; i++) {
						Vertex vert = buildVertex(i);
						auto it = uniqueVertices.find(vert);
						if (it == uniqueVertices.end()) {
							uint32_t newIndex = uint32_t(vertices.size());
							uniqueVertices[vert] = newIndex;
							vertices.push_back(vert);
							indices.push_back(newIndex);
						}
						else {
							indices.push_back(it->second);
						}
					}
				}
			}
		}

		// 5) Bake in the baseColorFactor
		for (auto& v : vertices) {
			v.color *= glm::vec3(baseColorFactor);
		}
	}

	HvkModel::HvkModel(HvkDevice& dev, Builder const& b)
		: device_(dev)
	{
		createVertexBuffers(b.vertices);
		createIndexBuffers(b.indices);
		if (b.hasBaseColor || b.hasMR || b.hasNormalMap || b.hasEmissive)
			createTextureResources(b);
	}

	HvkModel::~HvkModel() {
		for (auto s : samplers_) vkDestroySampler(device_.device(), s, nullptr);
		for (auto v : imageViews_) vkDestroyImageView(device_.device(), v, nullptr);
		for (auto im : images_) vkDestroyImage(device_.device(), im, nullptr);
		for (auto mem : imageMemories_) vkFreeMemory(device_.device(), mem, nullptr);
	}

	void HvkModel::createVertexBuffers(std::vector<Vertex> const& verts) {
		vertexCount_ = verts.size();
		HvkBuffer staging{ device_, sizeof(Vertex), vertexCount_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
							VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
		staging.map();
		// Fix: Cast the data pointer to void* to match the parameter type  
		staging.writeToBuffer(const_cast<void*>(reinterpret_cast<const void*>(verts.data())));
		vertexBuffer_ = std::make_unique<HvkBuffer>(device_, sizeof(Vertex), vertexCount_,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		device_.copyBuffer(staging.getBuffer(), vertexBuffer_->getBuffer(), sizeof(Vertex) * vertexCount_);
	}

	void HvkModel::createIndexBuffers(std::vector<uint32_t> const& inds) {
		indexCount_ = inds.size();
		if (!indexCount_) return;
		HvkBuffer staging{ device_, sizeof(uint32_t), indexCount_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
						   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
		staging.map();
		// Fix: Cast the data pointer to void* to match the parameter type
		staging.writeToBuffer(const_cast<void*>(reinterpret_cast<const void*>(inds.data())));
		indexBuffer_ = std::make_unique<HvkBuffer>(device_, sizeof(uint32_t), indexCount_,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		device_.copyBuffer(staging.getBuffer(), indexBuffer_->getBuffer(), sizeof(uint32_t) * indexCount_);
	}
	void HvkModel::createTextureResources(Builder const& b) {
		auto addImage = [&](auto const* img) {
			if (!img || img->width == 0 || img->height == 0) {
				// nothing to do
				return;
			}
			VkDeviceSize sz = img->width * img->height * 4;
			images_.push_back(VK_NULL_HANDLE);
			imageMemories_.push_back(VK_NULL_HANDLE);
			device_.createImageWithInfo({
				VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
				nullptr,
				0,
				VK_IMAGE_TYPE_2D,
				VK_FORMAT_R8G8B8A8_SRGB, // Corrected type to VkFormat  
				{uint32_t(img->width), uint32_t(img->height), 1}, // Fixed extent initialization  
				1,
				1,
				VK_SAMPLE_COUNT_1_BIT,
				VK_IMAGE_TILING_OPTIMAL,
				VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				VK_SHARING_MODE_EXCLUSIVE,
				0,
				nullptr,
				VK_IMAGE_LAYOUT_UNDEFINED
				}, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, images_.back(), imageMemories_.back());

			// Staging & copy  
			HvkBuffer st{
				device_,
				1,
				uint32_t(sz),
				VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
				VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
			};
			st.map();
			st.writeToBuffer((void*)img->image.data());

			// begin a single‐use command buffer
			VkCommandBuffer cmd = device_.beginSingleTimeCommands();

			// barrier: UNDEFINED → TRANSFER_DST_OPTIMAL
			hvk::transitionImageLayout(
				cmd,
				images_.back(),
				VK_FORMAT_R8G8B8A8_SRGB,
				VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
			);

			// 4) copy buffer → image *in the same cmd*
			VkBufferImageCopy copyRegion{};
			copyRegion.bufferOffset = 0;
			copyRegion.bufferRowLength = 0;
			copyRegion.bufferImageHeight = 0;
			copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.imageSubresource.mipLevel = 0;
			copyRegion.imageSubresource.baseArrayLayer = 0;
			copyRegion.imageSubresource.layerCount = 1;
			copyRegion.imageOffset = { 0, 0, 0 };
			copyRegion.imageExtent = {
				static_cast<uint32_t>(img->width),
				static_cast<uint32_t>(img->height),
				1
			};
			vkCmdCopyBufferToImage(
				cmd,
				st.getBuffer(),
				images_.back(),
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&copyRegion
			);

			// barrier: TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL
			hvk::transitionImageLayout(
				cmd,
				images_.back(),
				VK_FORMAT_R8G8B8A8_SRGB,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			);

			// submit & free that cmd buffer
			device_.endSingleTimeCommands(cmd);
			
			// View + sampler  
			VkImageViewCreateInfo vi{
				VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				nullptr,
				0,
				images_.back(),
				VK_IMAGE_VIEW_TYPE_2D,
				VK_FORMAT_R8G8B8A8_SRGB,
				{},
				{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
			};
			imageViews_.push_back(VK_NULL_HANDLE);
			vkCreateImageView(device_.device(), &vi, nullptr, &imageViews_.back());

			VkSamplerCreateInfo si{
				VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,  // sType
				nullptr,                                // pNext
				0,                                      // flags
				VK_FILTER_LINEAR,                       // magFilter
				VK_FILTER_LINEAR,                       // minFilter
				VK_SAMPLER_MIPMAP_MODE_LINEAR,          // mipmapMode
				VK_SAMPLER_ADDRESS_MODE_REPEAT,         // addressModeU
				VK_SAMPLER_ADDRESS_MODE_REPEAT,         // addressModeV
				VK_SAMPLER_ADDRESS_MODE_REPEAT,         // addressModeW
				0.0f,                                   // mipLodBias
				VK_TRUE,                                // anisotropyEnable
				16.0f,                                  // maxAnisotropy
				VK_FALSE,                               // compareEnable
				VK_COMPARE_OP_ALWAYS,                   // compareOp
				0.0f,                                   // minLod
				0.0f,                                   // maxLod
				VK_BORDER_COLOR_INT_OPAQUE_BLACK,       // borderColor
				VK_FALSE                                // unnormalizedCoordinates
			};
			samplers_.push_back(VK_NULL_HANDLE);
			vkCreateSampler(device_.device(), &si, nullptr, &samplers_.back());
			imageInfos_.push_back({ samplers_.back(), imageViews_.back(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL });
			};

		if (b.hasBaseColor) addImage(b.baseColorImage);
		if (b.hasMR)        addImage(b.mrImage);
		if (b.hasNormalMap) addImage(b.normalImage);
		if (b.hasEmissive)  addImage(b.emissiveImage);
	}

	void HvkModel::bind(VkCommandBuffer cmd, VkPipelineLayout layout) const {
		VkBuffer buf = vertexBuffer_->getBuffer(); VkDeviceSize off = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &buf, &off);
		if (indexCount_) vkCmdBindIndexBuffer(cmd, indexBuffer_->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
	}

	void HvkModel::draw(VkCommandBuffer cmd) const {
		if (indexCount_) vkCmdDrawIndexed(cmd, indexCount_, 1, 0, 0, 0);
		else            vkCmdDraw(cmd, vertexCount_, 1, 0, 0);
	}

	void HvkModel::writeDescriptors(VkDescriptorSet set) const {
		HvkDescriptorWriter writer(*descriptorSetLayout_, *descriptorPool_);
		// binding 0 = UBO already written in main
		for (size_t i = 0; i < imageInfos_.size(); i++) {
			// Cast away constness to match the parameter type
			writer.writeImage(uint32_t(1 + i), const_cast<VkDescriptorImageInfo*>(&imageInfos_[i]));
		}
		writer.build(set);
	}

} // namespace hvk