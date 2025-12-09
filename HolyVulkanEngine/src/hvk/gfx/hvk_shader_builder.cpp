#include <hvk/gfx/hvk_shader_builder.hpp>
#include <hvk/gfx/hvk_device.h>
#include <fstream>
#include <stdexcept>
#include <cstring>

namespace hvk {

ShaderBuilder& ShaderBuilder::loadVertex(const char* path, const char* entryPoint) {
    shaders_.push_back({path, entryPoint, VK_SHADER_STAGE_VERTEX_BIT});
    return *this;
}

ShaderBuilder& ShaderBuilder::loadFragment(const char* path, const char* entryPoint) {
    shaders_.push_back({path, entryPoint, VK_SHADER_STAGE_FRAGMENT_BIT});
    return *this;
}

ShaderBuilder& ShaderBuilder::loadCompute(const char* path, const char* entryPoint) {
    shaders_.push_back({path, entryPoint, VK_SHADER_STAGE_COMPUTE_BIT});
    return *this;
}

ShaderBuilder& ShaderBuilder::loadGeometry(const char* path, const char* entryPoint) {
    shaders_.push_back({path, entryPoint, VK_SHADER_STAGE_GEOMETRY_BIT});
    return *this;
}

ShaderBuilder& ShaderBuilder::loadTessControl(const char* path, const char* entryPoint) {
    shaders_.push_back({path, entryPoint, VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT});
    return *this;
}

ShaderBuilder& ShaderBuilder::loadTessEval(const char* path, const char* entryPoint) {
    shaders_.push_back({path, entryPoint, VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT});
    return *this;
}

ShaderModules ShaderBuilder::build(const Device& device) {
    if (shaders_.empty()) {
        throw std::runtime_error("ShaderBuilder::build() - No shaders loaded!");
    }

    ShaderModules result;

    for (const auto& shaderInfo : shaders_) {
        // Load SPIR-V bytecode
        auto spirv = loadSpirv(shaderInfo.path.c_str());

        // Create shader module
        VkShaderModule module = createShaderModule(
            device.device(),
            spirv,
            shaderInfo.path.c_str()
        );

        // Store module in appropriate slot
        switch (shaderInfo.stage) {
            case VK_SHADER_STAGE_VERTEX_BIT:
                result.vertex = module;
                break;
            case VK_SHADER_STAGE_FRAGMENT_BIT:
                result.fragment = module;
                break;
            case VK_SHADER_STAGE_COMPUTE_BIT:
                result.compute = module;
                break;
            case VK_SHADER_STAGE_GEOMETRY_BIT:
                result.geometry = module;
                break;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
                result.tessControl = module;
                break;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
                result.tessEval = module;
                break;
            default:
                throw std::runtime_error("ShaderBuilder::build() - Unknown shader stage!");
        }

        // Create stage descriptor
        ShaderStageDesc stageDesc{};
        stageDesc.stage = static_cast<VkShaderStageFlagBits>(shaderInfo.stage);
        stageDesc.module = module;
        stageDesc.entry = shaderInfo.entryPoint;

        result.stages.push_back(stageDesc);
    }

    return result;
}

std::vector<uint32_t> ShaderBuilder::loadSpirv(const char* path) {
    // Open file in binary mode
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        throw std::runtime_error(std::string("ShaderBuilder: Failed to open SPIR-V file: ") + path);
    }

    // Get file size
    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0) {
        throw std::runtime_error(std::string("ShaderBuilder: SPIR-V file is empty: ") + path);
    }

    // Validate file size is multiple of 4 (SPIR-V requirement)
    if (fileSize % 4 != 0) {
        throw std::runtime_error(std::string("ShaderBuilder: SPIR-V file size not multiple of 4: ") + path);
    }

    // Read file into byte buffer
    file.seekg(0);
    std::vector<uint8_t> bytes(fileSize);
    file.read(reinterpret_cast<char*>(bytes.data()), fileSize);

    if (!file) {
        throw std::runtime_error(std::string("ShaderBuilder: Failed to read SPIR-V file: ") + path);
    }

    // Convert to uint32_t array
    std::vector<uint32_t> code(fileSize / 4);
    std::memcpy(code.data(), bytes.data(), fileSize);

    // Validate SPIR-V magic number
    if (code.empty() || code[0] != 0x07230203) {
        throw std::runtime_error(std::string("ShaderBuilder: Invalid SPIR-V magic number in file: ") + path);
    }

    return code;
}

VkShaderModule ShaderBuilder::createShaderModule(
    VkDevice device,
    const std::vector<uint32_t>& code,
    const char* debugName
) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);

    if (result != VK_SUCCESS) {
        std::string errorMsg = "ShaderBuilder: Failed to create shader module";
        if (debugName) {
            errorMsg += std::string(" for: ") + debugName;
        }
        throw std::runtime_error(errorMsg);
    }

    return shaderModule;
}

} // namespace hvk
