#include "vulkan_engine.h"

void VulkanEngine::init() {
    LOG_METHOD();

    init_window();
    init_vulkan();
}

void VulkanEngine::shutdown() {
    LOG_METHOD();

    cleanup();
}

void VulkanEngine::cleanup() {
    LOG_METHOD();

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_enable_validation_layers && m_debug_messenger != VK_NULL_HANDLE) {
        destroy_debug_utils_messenger_ext(m_instance, m_debug_messenger, nullptr);
        m_debug_messenger = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }

    glfwTerminate();
}

void VulkanEngine::init_window() {
    LOG_METHOD();

    logger.assert(glfwInit(), "Failed to initialize GLFW");

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        m_window_title,
        nullptr,
        nullptr
    );

    logger.assert(m_window, "Failed to create GLFW window");
}

void VulkanEngine::init_vulkan() {
    LOG_METHOD();

    create_instance();
    setup_debug_messenger();
    create_surface();

    pick_physical_device();
    create_logical_device();
}

VulkanEngine::QueueFamilyIndices VulkanEngine::find_queue_families(VkPhysicalDevice device) const {
    LOG_METHOD();

    QueueFamilyIndices indices;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        const auto& queue_family = queue_families[i];

        if (queue_family.queueCount > 0 && (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
            indices.graphics_family = i;
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);

        if (queue_family.queueCount > 0 && present_support) {
            indices.present_family = i;
        }

        if (indices.is_complete()) {
            break;
        }
    }

    return indices;
}

bool VulkanEngine::check_validation_layer_support() const {
    LOG_METHOD();

    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    for (const char* required_layer_name : m_validationLayers) {
        bool found = false;

        for (const auto& available_layer : available_layers) {
            if (std::string(required_layer_name) == available_layer.layerName) {
                found = true;
                break;
            }
        }

        if (!found) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanEngine::get_required_extensions() const {
    LOG_METHOD();

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    logger.assert(glfw_extensions != nullptr, "glfwGetRequiredInstanceExtensions returned nullptr");

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

    if (m_enable_validation_layers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void VulkanEngine::create_instance() {
    LOG_METHOD();

    if (m_enable_validation_layers) {
        logger.assert(
            check_validation_layer_support(),
            "Validation layers requested, but not available"
        );
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = m_window_title;
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "VulkanEngine";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = get_required_extensions();

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    if (m_enable_validation_layers) {
        create_info.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        create_info.ppEnabledLayerNames = m_validationLayers.data();
    } else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
    }

    VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
    logger.assert(result == VK_SUCCESS, "Failed to create Vulkan instance");
}

void VulkanEngine::setup_debug_messenger() {
    LOG_METHOD();

    if (!m_enable_validation_layers) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    populate_debug_messenger_create_info(create_info);

    VkResult result = create_debug_utils_messenger_ext(
        m_instance,
        &create_info,
        nullptr,
        &m_debug_messenger
    );

    logger.assert(result == VK_SUCCESS, "Failed to set up debug messenger");
}

void VulkanEngine::create_surface() {
    LOG_METHOD();

    VkResult result = glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface);
    logger.assert(result == VK_SUCCESS, "Failed to create window surface");
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanEngine::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    (void)message_severity;
    (void)message_type;
    (void)user_data;

    logger.log_traceback();

    std::cerr << ColoredString("[VULKAN VALIDATION ERROR] ", LoggerPalette::error)
              << callback_data->pMessage
              << std::endl;

    return VK_FALSE;
}

void VulkanEngine::populate_debug_messenger_create_info(
    VkDebugUtilsMessengerCreateInfoEXT& create_info) const
{
    create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    create_info.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    create_info.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    create_info.pfnUserCallback = debug_callback;
    create_info.pUserData = nullptr;
}

VkResult VulkanEngine::create_debug_utils_messenger_ext(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* create_info,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* debug_messenger)
{
    LOG_NAMED("VulkanEngine");

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );

    if (func != nullptr) {
        return func(instance, create_info, allocator, debug_messenger);
    }

    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void VulkanEngine::destroy_debug_utils_messenger_ext(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debug_messenger,
    const VkAllocationCallbacks* allocator)
{
    LOG_NAMED("VulkanEngine");

    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
    );

    if (func != nullptr) {
        func(instance, debug_messenger, allocator);
    }
}
