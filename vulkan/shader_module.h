#pragma once
#include <filesystem>
#include <vulkan/vulkan.h>
#include <vector>
#include <fstream>
#include "vulkan_utils.h"


class ShaderModule {
public:
    VkShaderModule shader_module;
    ShaderModule() = default;
    ShaderModule(VkDevice& device, const std::filesystem::path& path);

    std::vector<char> read_file(const std::string& filename);
    void create(VkDevice& device, const std::vector<char>& code);
    void create(VkDevice& device, const std::string& filename);
};