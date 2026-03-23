#include "shader_module.h"

ShaderModule::ShaderModule(VkDevice& device, const std::filesystem::path& path) {
    create(device, path);
}

std::vector<char> ShaderModule::read_file(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("failed to open file: " + filename);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);

    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));
    file.close();

    return buffer;
}

void ShaderModule::create(VkDevice& device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    shader_module = VK_NULL_HANDLE;
    vulkan_utils::vk_check(
        vkCreateShaderModule(device, &createInfo, nullptr, &shader_module),
        "vkCreateShaderModule"
    );
}

void ShaderModule::create(VkDevice& device, const std::string& filename) {
    std::vector<char> code = read_file(filename);
    create(device, code);
}