#include "pch.h"

#include <hvk/gfx/hvk_shader_reflect.h>

#if defined(HVK_USE_SPIRV_REFLECT)
#  include "spirv_reflect.h" // https://github.com/KhronosGroup/SPIRV-Reflect (single-header C lib)
#endif

#ifndef VK_CHECK
#define VK_CHECK(x) do { VkResult _e = (x); if (_e != VK_SUCCESS) throw std::runtime_error("Vulkan error: " #x); } while(0)
#endif

namespace hvk {

    // ========================= ShaderModule ======================================

    void ShaderModule::init(const ShaderModuleCreateInfo& ci) {
        if (!ci.device || !ci.spirv || ci.spirvWordCount == 0)
            throw std::invalid_argument("ShaderModule: invalid create info");
        device_ = ci.device;
        stage_ = ci.stage;
        entry_ = ci.entry;

        VkShaderModuleCreateInfo mi{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
        mi.codeSize = ci.spirvWordCount * sizeof(uint32_t);
        mi.pCode = ci.spirv;
        VK_CHECK(vkCreateShaderModule(device_->device(), &mi, nullptr, &module_));

        if (!ci.debugName.empty()) {
            device_->setObjectName(VK_OBJECT_TYPE_SHADER_MODULE,
                reinterpret_cast<uint64_t>(module_), ci.debugName);
        }
    }

    void ShaderModule::destroy() {
        if (module_) {
            vkDestroyShaderModule(device_->device(), module_, nullptr);
            module_ = VK_NULL_HANDLE;
        }
        device_ = nullptr;
        entry_.clear();
        stage_ = VK_SHADER_STAGE_VERTEX_BIT;
    }

    // ========================= Helpers ===========================================

    static VkDescriptorType map_desc_type(uint32_t spv_reflect_desc_type) {
#if defined(HVK_USE_SPIRV_REFLECT)
        switch (spv_reflect_desc_type) {
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:                return VK_DESCRIPTOR_TYPE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:          return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:   return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:   return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:       return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        }
#endif
        return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }

    static VkFormat map_vertex_format(uint32_t spv_format) {
#if defined(HVK_USE_SPIRV_REFLECT)
        switch (spv_format) {
        case SPV_REFLECT_FORMAT_R32_SFLOAT:          return VK_FORMAT_R32_SFLOAT;
        case SPV_REFLECT_FORMAT_R32G32_SFLOAT:       return VK_FORMAT_R32G32_SFLOAT;
        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:    return VK_FORMAT_R32G32B32_SFLOAT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case SPV_REFLECT_FORMAT_R32_UINT:            return VK_FORMAT_R32_UINT;
        case SPV_REFLECT_FORMAT_R32G32_UINT:         return VK_FORMAT_R32G32_UINT;
        case SPV_REFLECT_FORMAT_R32G32B32_UINT:      return VK_FORMAT_R32G32B32_UINT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:   return VK_FORMAT_R32G32B32A32_UINT;
        case SPV_REFLECT_FORMAT_R32_SINT:            return VK_FORMAT_R32_SINT;
        case SPV_REFLECT_FORMAT_R32G32_SINT:         return VK_FORMAT_R32G32_SINT;
        case SPV_REFLECT_FORMAT_R32G32B32_SINT:      return VK_FORMAT_R32G32B32_SINT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:   return VK_FORMAT_R32G32B32A32_SINT;
            // add more as needed; most vertex attribs are covered by the common R32 types
        }
#endif
        return VK_FORMAT_UNDEFINED;
    }

    // ========================= ShaderReflector::reflect ===========================

    ModuleReflection ShaderReflector::reflect(const uint32_t* spirv, size_t wordCount,
        VkShaderStageFlagBits stage)
    {
#if !defined(HVK_USE_SPIRV_REFLECT)
        (void)spirv; (void)wordCount; (void)stage;
        throw std::runtime_error("ShaderReflector::reflect requires HVK_USE_SPIRV_REFLECT.");
#else
        SpvReflectShaderModule mod;
        SpvReflectResult rr = spvReflectCreateShaderModule(wordCount * sizeof(uint32_t), spirv, &mod);
        if (rr != SPV_REFLECT_RESULT_SUCCESS) throw std::runtime_error("SPIRV-Reflect: create module failed");

        ModuleReflection out{};
        out.stage = stage;

        // ---- descriptor sets ----
        uint32_t setCount = 0;
        spvReflectEnumerateDescriptorSets(&mod, &setCount, nullptr);
        std::vector<SpvReflectDescriptorSet*> sets(setCount);
        spvReflectEnumerateDescriptorSets(&mod, &setCount, sets.data());

        out.sets.reserve(setCount);
        for (auto* s : sets) {
            ReflectedSet rs{};
            rs.set = s->set;
            rs.bindings.reserve(s->binding_count);
            for (uint32_t bi = 0; bi < s->binding_count; ++bi) {
                auto* b = s->bindings[bi];
                ReflectedBinding rb{};
                rb.set = s->set;
                rb.binding = b->binding;
                rb.type = map_desc_type(b->descriptor_type);
                rb.count = (b->count > 0) ? b->count : 0; // runtime array -> 0
                rb.runtimeArray = (b->count == 0);
                rb.stages = stage;
                rs.bindings.push_back(rb);
            }
            out.sets.push_back(std::move(rs));
        }

        // ---- push constants ----
        uint32_t pcbCount = 0;
        spvReflectEnumeratePushConstantBlocks(&mod, &pcbCount, nullptr);
        std::vector<SpvReflectBlockVariable*> pcbs(pcbCount);
        spvReflectEnumeratePushConstantBlocks(&mod, &pcbCount, pcbs.data());
        out.pushes.reserve(pcbCount);
        for (auto* p : pcbs) {
            ReflectedPushConstant pc{};
            pc.offset = p->offset;
            pc.size = p->size;
            pc.stages = stage;
            out.pushes.push_back(pc);
        }

        // ---- specialization constants (optional) ----
        uint32_t scCount = 0;
        spvReflectEnumerateSpecializationConstants(&mod, &scCount, nullptr);
        std::vector<SpvReflectSpecializationConstant*> scs(scCount);
        spvReflectEnumerateSpecializationConstants(&mod, &scCount, scs.data());
        out.specs.reserve(scCount);
        for (auto* sc : scs) {
            ReflectedSpecConstant rc{};
            rc.constantID = sc->constant_id;
            rc.offset = sc->constant.offset;
            rc.size = sc->constant.size;
            out.specs.push_back(rc);
        }

        // ---- vertex inputs (VS only; skip builtins) ----
        if (stage == VK_SHADER_STAGE_VERTEX_BIT) {
            uint32_t inCount = 0;
            spvReflectEnumerateInputVariables(&mod, &inCount, nullptr);
            std::vector<SpvReflectInterfaceVariable*> ivs(inCount);
            spvReflectEnumerateInputVariables(&mod, &inCount, ivs.data());
            for (auto* iv : ivs) {
                if (iv->built_in != -1) continue;   // skip gl_VertexIndex, etc.
                if (iv->location < 0) continue;
                ReflectedVertexAttr a{};
                a.location = static_cast<uint32_t>(iv->location);
                a.format = map_vertex_format(iv->format);
                out.vertexInputs.push_back(a);
            }
            std::sort(out.vertexInputs.begin(), out.vertexInputs.end(),
                [](const ReflectedVertexAttr& A, const ReflectedVertexAttr& B) { return A.location < B.location; });
        }

        spvReflectDestroyShaderModule(&mod);
        return out;
#endif
    }

    // ========================= merge helpers =====================================

    static void upsert_binding(std::vector<ReflectedBinding>& list, const ReflectedBinding& in) {
        for (auto& b : list) {
            if (b.binding == in.binding && b.set == in.set) {
                b.stages |= in.stages;
                // Prefer the largest count in case stages disagree (shouldn’t happen in valid SPIR-V)
                b.count = std::max(b.count, in.count);
                b.runtimeArray = b.runtimeArray || in.runtimeArray;
                return;
            }
        }
        list.push_back(in);
    }

    static std::optional<VkDescriptorBindingFlags> find_binding_flags(const ReflectionOverrides& ov, uint32_t set, uint32_t binding, std::optional<uint32_t>& forcedCount) {
        for (const auto& bo : ov.bindingOverrides) {
            if (bo.set == set && bo.binding == binding) {
                forcedCount = bo.forceDescriptorCount;
                return bo.flags;
            }
        }
        return std::nullopt;
    }

    static bool set_update_after_bind(const ReflectionOverrides& ov, uint32_t set) {
        for (const auto& so : ov.setOverrides) if (so.set == set) return so.updateAfterBindPool;
        return false;
    }

    void ShaderReflector::buildSetLayouts(const Device& device,
        const std::vector<ModuleReflection>& stages,
        const ReflectionOverrides& overrides,
        std::vector<DescriptorSetLayout>& outLayouts,
        std::vector<uint32_t>& outSetIndices)
    {
        // Merge bindings per set index
        std::unordered_map<uint32_t, std::vector<ReflectedBinding>> bySet;
        for (const auto& s : stages) {
            for (const auto& set : s.sets) {
                auto& vec = bySet[set.set];
                for (const auto& b : set.bindings) upsert_binding(vec, b);
            }
        }

        outLayouts.clear();
        outSetIndices.clear();
        outLayouts.reserve(bySet.size());
        outSetIndices.reserve(bySet.size());

        for (auto& kv : bySet) {
            uint32_t setIdx = kv.first;
            auto& bindingsMerged = kv.second;

            // Build VkDescriptorSetLayoutBinding + optional flags per binding
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            std::vector<VkDescriptorBindingFlags>     flags; // optional
            bindings.reserve(bindingsMerged.size());
            flags.reserve(bindingsMerged.size());

            bool anyFlags = false;
            bool updateAfterBindPool = set_update_after_bind(overrides, setIdx);

            for (auto& rb : bindingsMerged) {
                VkDescriptorSetLayoutBinding b{};
                b.binding = rb.binding;
                b.descriptorType = rb.type;
                b.descriptorCount = (rb.runtimeArray) ? 1u : std::max(1u, rb.count);
                b.stageFlags = rb.stages;
                b.pImmutableSamplers = nullptr;

                bindings.push_back(b);

                // Apply overrides for binding flags (including VARIABLE_DESCRIPTOR_COUNT for runtime arrays)
                std::optional<uint32_t> forcedCount;
                auto fl = find_binding_flags(overrides, setIdx, rb.binding, forcedCount);
                VkDescriptorBindingFlags bf = 0;
                if (fl.has_value()) {
                    bf |= *fl;
                    anyFlags = true;
                    if ((bf & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT) && forcedCount.has_value()) {
                        bindings.back().descriptorCount = *forcedCount; // layout requires a concrete max
                    }
                }
                else if (rb.runtimeArray) {
                    // If runtime array and no explicit override, suggest VARIABLE_DESCRIPTOR_COUNT
                    // (engine may choose to not use bindless — then leave as 1).
                    // We default to no special flag unless user opted-in via overrides.
                }
                flags.push_back(bf);
            }

            DescriptorSetLayoutCreateInfo lci{};
            lci.device = &device;
            lci.bindings = std::move(bindings);
            if (anyFlags) lci.bindingFlags = std::move(flags);
            lci.updateAfterBindPool = updateAfterBindPool;
            lci.debugName = std::string("set_") + std::to_string(setIdx);

            outSetIndices.push_back(setIdx);
            outLayouts.emplace_back(lci);
        }

        // Keep outputs ordered by set index for deterministic pipeline layout creation
        std::vector<size_t> order(outSetIndices.size());
        for (size_t i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return outSetIndices[a] < outSetIndices[b]; });

        std::vector<DescriptorSetLayout> layoutsOrdered;
        std::vector<uint32_t>            setsOrdered;
        layoutsOrdered.reserve(order.size());
        setsOrdered.reserve(order.size());
        for (auto i : order) {
            layoutsOrdered.push_back(std::move(outLayouts[i]));
            setsOrdered.push_back(outSetIndices[i]);
        }
        outLayouts = std::move(layoutsOrdered);
        outSetIndices = std::move(setsOrdered);
    }

    std::vector<VkPushConstantRange> ShaderReflector::mergePushConstants(const std::vector<ModuleReflection>& stages) {
        // Simple union by (offset,size). If two blocks overlap, merge & union stage flags.
        struct R { uint32_t off, size; VkShaderStageFlags stages; };
        std::vector<R> ranges;

        for (const auto& s : stages) {
            for (const auto& pc : s.pushes) {
                bool merged = false;
                for (auto& r : ranges) {
                    const uint32_t a0 = r.off, a1 = r.off + r.size;
                    const uint32_t b0 = pc.offset, b1 = pc.offset + pc.size;
                    const bool overlap = !(b1 <= a0 || b0 >= a1);
                    if (overlap || (a0 == b0 && a1 == b1)) {
                        const uint32_t lo = std::min(a0, b0);
                        const uint32_t hi = std::max(a1, b1);
                        r.off = lo; r.size = hi - lo;
                        r.stages |= pc.stages;
                        merged = true;
                        break;
                    }
                }
                if (!merged) ranges.push_back({ pc.offset, pc.size, pc.stages });
            }
        }

        std::vector<VkPushConstantRange> out;
        out.reserve(ranges.size());
        for (auto& r : ranges) {
            VkPushConstantRange pr{};
            pr.offset = r.off;
            pr.size = r.size;
            pr.stageFlags = r.stages;
            out.push_back(pr);
        }
        // Sort by offset for determinism
        std::sort(out.begin(), out.end(), [](const auto& A, const auto& B) { return A.offset < B.offset; });
        return out;
    }

    VkPipelineLayout ShaderReflector::makePipelineLayout(PipelineLayoutCache& cache,
        const std::vector<ModuleReflection>& stages,
        const ReflectionOverrides& overrides)
    {
        std::vector<DescriptorSetLayout> layouts;
        std::vector<uint32_t> setIdx;
        buildSetLayouts(*cache.device(), stages, overrides, layouts, setIdx);

        std::vector<VkDescriptorSetLayout> vkLayouts;
        vkLayouts.reserve(layouts.size());
        for (auto& l : layouts) vkLayouts.push_back(l.handle());

        auto push = mergePushConstants(stages);

        PipelineLayoutDesc pld{};
        pld.setLayouts = std::move(vkLayouts);
        pld.pushConstants = std::move(push);

        return cache.get(pld);
    }

    // Vertex input builder: patch offsets/stride/binding.
    VertexInputDesc ShaderReflector::makeVertexInput(
        const ModuleReflection& vsRefl,
        const std::unordered_map<uint32_t, uint32_t>& locationToOffset,
        uint32_t binding,
        uint32_t stride,
        VkVertexInputRate rate)
    {
        VertexInputDesc vi{};
        // Binding
        VkVertexInputBindingDescription bd{};
        bd.binding = binding;
        bd.inputRate = rate;
        bd.stride = stride;
        vi.bindings.push_back(bd);

        // Attributes: order by location
        for (const auto& a : vsRefl.vertexInputs) {
            VkVertexInputAttributeDescription ad{};
            ad.location = a.location;
            ad.binding = binding;
            ad.format = a.format;
            auto it = locationToOffset.find(a.location);
            if (it == locationToOffset.end()) {
                throw std::runtime_error("makeVertexInput: missing offset for location " + std::to_string(a.location));
            }
            ad.offset = it->second;
            vi.attributes.push_back(ad);
        }
        return vi;
    }

} // namespace hvk
