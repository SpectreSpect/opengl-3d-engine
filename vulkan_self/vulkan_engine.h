#pragma once

#include <set>
#include <vector>
#include <string>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "logger/logger_header.h"

class VulkanEngine {
public:
    _XCLASS_NAME(VulkanEngine);

    void init();
    void shutdown();
    void cleanup();

    void init_window();
    void init_vulkan();

private:
    void create_instance();
    void setup_debug_messenger();
    void create_surface();

    void pickPhysicalDevice();
    void createLogicalDevice();

    void createSwapchain();
    void createImageViews();

    void createCommandPool();
    void createCommandBuffers();

    void createSyncObjects();

private:
    GLFWwindow* m_window = nullptr;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    const uint32_t m_width = 1280;
    const uint32_t m_height = 720;
    const char* m_window_title = "VulkanEngine";

private:
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_present_queue = VK_NULL_HANDLE;

    const std::vector<const char*> m_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

private:
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphics_family;
        std::optional<uint32_t> present_family;

        bool is_complete() const {
            return graphics_family.has_value() && present_family.has_value();
        }
    };

private:
    void pick_physical_device();
    void create_logical_device();

    bool is_device_suitable(VkPhysicalDevice device) const;
    bool check_device_extension_support(VkPhysicalDevice device) const;
    QueueFamilyIndices find_queue_families(VkPhysicalDevice device) const;

private:

#ifdef NDEBUG
    const bool m_enableValidationLayers = false;
#else
    const bool m_enable_validation_layers = true;
#endif

    const std::vector<const char*> m_validationLayers = {
        "VK_LAYER_KHRONOS_validation"
    };

private:
    bool check_validation_layer_support() const;
    std::vector<const char*> get_required_extensions() const;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data
    );

    void populate_debug_messenger_create_info(
        VkDebugUtilsMessengerCreateInfoEXT& create_info
    ) const;

    static VkResult create_debug_utils_messenger_ext(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT* create_info,
        const VkAllocationCallbacks* allocator,
        VkDebugUtilsMessengerEXT* debug_messenger
    );

    static void destroy_debug_utils_messenger_ext(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debug_messenger,
        const VkAllocationCallbacks* allocator
    );
};
