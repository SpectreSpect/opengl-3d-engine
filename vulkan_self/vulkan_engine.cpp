#include "vulkan_engine.h"

VulkanEngine::VulkanEngine(Window& window) : m_window(window) {
    
}

VulkanEngine::~VulkanEngine() {

}

void VulkanEngine::init() {
    LOG_METHOD();

    init_vulkan();
}

void VulkanEngine::shutdown() {
    LOG_METHOD();

    cleanup();
}

void VulkanEngine::cleanup() {
    LOG_METHOD();

    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }

    for (VkSemaphore semaphore : m_render_finished_semaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, semaphore, nullptr);
        }
    }

    for (VkSemaphore semaphore : m_image_available_semaphores) {
        if (semaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, semaphore, nullptr);
        }
    }

    for (VkFence fence : m_in_flight_fences) {
        if (fence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, fence, nullptr);
        }
    }

    m_render_finished_semaphores.clear();
    m_image_available_semaphores.clear();
    m_in_flight_fences.clear();

    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }

    for (VkFramebuffer framebuffer : m_swapchain_framebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    m_swapchain_framebuffers.clear();

    if (m_render_pass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device, m_render_pass, nullptr);
        m_render_pass = VK_NULL_HANDLE;
    }

    for (VkImageView image_view : m_swapchain_image_views) {
        vkDestroyImageView(m_device, image_view, nullptr);
    }
    m_swapchain_image_views.clear();

    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

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
}

void VulkanEngine::init_vulkan() {
    LOG_METHOD();

    logger.log() << "glfwVulkanSupported: " << clr(std::to_string(glfwVulkanSupported()), "#2c87ff") << "\n";

    logger.check(glfwVulkanSupported(), "GLFW reports Vulkan is not supported on this machine");

    create_instance();
    setup_debug_messenger();
    create_surface();

    pick_physical_device();
    create_logical_device();

    create_swapchain();
    create_image_views();

    create_render_pass();
    create_framebuffers();

    create_command_pool();
    create_command_buffers();
    create_sync_objects();
}

void VulkanEngine::run() {
    LOG_METHOD();

    while (!m_window.should_close()) {
        m_window.poll_events();
        draw_frame();
    }

    vkDeviceWaitIdle(m_device);
}

void VulkanEngine::pick_physical_device() {
    LOG_METHOD();

    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);

    logger.check(device_count > 0, "Failed to find GPUs with Vulkan support");

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.data());

    for (const auto& device : devices) {
        if (is_device_suitable(device)) {
            m_physical_device = device;
            break;
        }
    }

    logger.check(
        m_physical_device != VK_NULL_HANDLE,
        "Failed to find a suitable GPU"
    );
}

void VulkanEngine::create_logical_device() {
    LOG_METHOD();

    QueueFamilyIndices indices = find_queue_families(m_physical_device);

    std::set<uint32_t> unique_queue_families = {
        indices.graphics_family.value(),
        indices.present_family.value()
    };

    float queue_priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;

    for (uint32_t queue_family : unique_queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;

        queue_create_infos.push_back(queue_create_info);
    }

    VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;

    create_info.enabledExtensionCount = static_cast<uint32_t>(m_device_extensions.size());
    create_info.ppEnabledExtensionNames = m_device_extensions.data();

    if (m_enable_validation_layers) {
        create_info.enabledLayerCount = static_cast<uint32_t>(m_validationLayers.size());
        create_info.ppEnabledLayerNames = m_validationLayers.data();
    } else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
    }

    VkResult result = vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device);
    logger.check(result == VK_SUCCESS, "Failed to create logical device");

    vkGetDeviceQueue(m_device, indices.graphics_family.value(), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, indices.present_family.value(), 0, &m_present_queue);
}

bool VulkanEngine::is_device_suitable(VkPhysicalDevice device) const {
    LOG_METHOD();

    QueueFamilyIndices indices = find_queue_families(device);
    bool extensions_supported = check_device_extension_support(device);

    bool swapchain_adequate = false;
    if (extensions_supported) {
        SwapChainSupportDetails swapchain_support = query_swapchain_support(device);
        swapchain_adequate =
            !swapchain_support.formats.empty() &&
            !swapchain_support.present_modes.empty();
    }

    return indices.is_complete() && extensions_supported && swapchain_adequate;
}

bool VulkanEngine::check_device_extension_support(VkPhysicalDevice device) const {
    LOG_METHOD();

    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(
        device,
        nullptr,
        &extension_count,
        available_extensions.data()
    );

    std::set<std::string> required_extensions(
        m_device_extensions.begin(),
        m_device_extensions.end()
    );

    for (const auto& extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
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

void VulkanEngine::create_render_pass() {
    LOG_METHOD();

    VkAttachmentDescription color_attachment{};
    color_attachment.format = m_swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;

    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;

    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    VkResult result = vkCreateRenderPass(
        m_device,
        &render_pass_info,
        nullptr,
        &m_render_pass
    );

    logger.check(result == VK_SUCCESS, "Failed to create render pass");
}

void VulkanEngine::create_framebuffers() {
    LOG_METHOD();

    m_swapchain_framebuffers.resize(m_swapchain_image_views.size());

    for (size_t i = 0; i < m_swapchain_image_views.size(); ++i) {
        VkImageView attachments[] = {
            m_swapchain_image_views[i]
        };

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = m_render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = m_swapchainExtent.width;
        framebuffer_info.height = m_swapchainExtent.height;
        framebuffer_info.layers = 1;

        VkResult result = vkCreateFramebuffer(
            m_device,
            &framebuffer_info,
            nullptr,
            &m_swapchain_framebuffers[i]
        );

        logger.check(result == VK_SUCCESS, "Failed to create framebuffer");
    }
}

VulkanEngine::SwapChainSupportDetails VulkanEngine::query_swapchain_support(VkPhysicalDevice device) const {
    LOG_METHOD();

    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device,
        m_surface,
        &details.capabilities
    );

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);

    if (format_count != 0) {
        details.formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device,
            m_surface,
            &format_count,
            details.formats.data()
        );
    }

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device,
        m_surface,
        &present_mode_count,
        nullptr
    );

    if (present_mode_count != 0) {
        details.present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device,
            m_surface,
            &present_mode_count,
            details.present_modes.data()
        );
    }

    return details;
}

VkSurfaceFormatKHR VulkanEngine::choose_swap_surface_format(
    const std::vector<VkSurfaceFormatKHR>& available_formats
) const {
    LOG_METHOD();

    for (const auto& available_format : available_formats) {
        if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return available_format;
        }
    }

    return available_formats[0];
}

VkPresentModeKHR VulkanEngine::choose_swap_present_mode(
    const std::vector<VkPresentModeKHR>& available_present_modes
) const {
    LOG_METHOD();

    for (const auto& available_present_mode : available_present_modes) {
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return available_present_mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanEngine::choose_swap_extent(
    const VkSurfaceCapabilitiesKHR& capabilities
) const {
    LOG_METHOD();

    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(m_window.handle(), &width, &height);

    VkExtent2D actual_extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    actual_extent.width = std::clamp(
        actual_extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width
    );

    actual_extent.height = std::clamp(
        actual_extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height
    );

    return actual_extent;
}

void VulkanEngine::create_swapchain() {
    LOG_METHOD();

    SwapChainSupportDetails swapchain_support = query_swapchain_support(m_physical_device);
    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(swapchain_support.formats);
    VkPresentModeKHR present_mode = choose_swap_present_mode(swapchain_support.present_modes);
    VkExtent2D extent = choose_swap_extent(swapchain_support.capabilities);

    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;

    if (swapchain_support.capabilities.maxImageCount > 0 &&
        image_count > swapchain_support.capabilities.maxImageCount) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface;

    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = find_queue_families(m_physical_device);
    uint32_t queue_family_indices[] = {
        indices.graphics_family.value(),
        indices.present_family.value()
    };

    if (indices.graphics_family != indices.present_family) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    create_info.preTransform = swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VkResult result = vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swapchain);
    logger.check(result == VK_SUCCESS, "Failed to create swapchain");

    vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
    m_swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(
        m_device,
        m_swapchain,
        &image_count,
        m_swapchain_images.data()
    );

    m_swapchain_image_format = surface_format.format;
    m_swapchainExtent = extent;
}

void VulkanEngine::create_image_views() {
    LOG_METHOD();

    m_swapchain_image_views.resize(m_swapchain_images.size());

    for (size_t i = 0; i < m_swapchain_images.size(); ++i) {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = m_swapchain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = m_swapchain_image_format;

        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VkResult result = vkCreateImageView(
            m_device,
            &create_info,
            nullptr,
            &m_swapchain_image_views[i]
        );

        logger.check(result == VK_SUCCESS, "Failed to create swapchain image view");
    }
}

void VulkanEngine::create_command_pool() {
    LOG_METHOD();

    QueueFamilyIndices queue_family_indices = find_queue_families(m_physical_device);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();

    VkResult result = vkCreateCommandPool(
        m_device,
        &pool_info,
        nullptr,
        &m_commandPool
    );

    logger.check(result == VK_SUCCESS, "Failed to create command pool");
}

void VulkanEngine::create_command_buffers() {
    LOG_METHOD();

    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_commandPool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    VkResult result = vkAllocateCommandBuffers(
        m_device,
        &alloc_info,
        m_commandBuffers.data()
    );

    logger.check(result == VK_SUCCESS, "Failed to allocate command buffers");
}

void VulkanEngine::create_sync_objects() {
    LOG_METHOD();

    m_image_available_semaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_in_flight_fences.resize(MAX_FRAMES_IN_FLIGHT);

    // Важно: эти semaphores ждёт vkQueuePresentKHR,
    // поэтому они должны быть по количеству swapchain images.
    m_render_finished_semaphores.resize(m_swapchain_images.size());

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkResult image_semaphore_result = vkCreateSemaphore(
            m_device,
            &semaphore_info,
            nullptr,
            &m_image_available_semaphores[i]
        );

        VkResult fence_result = vkCreateFence(
            m_device,
            &fence_info,
            nullptr,
            &m_in_flight_fences[i]
        );

        logger.check(
            image_semaphore_result == VK_SUCCESS &&
            fence_result == VK_SUCCESS,
            "Failed to create per-frame synchronization objects"
        );
    }

    for (size_t i = 0; i < m_render_finished_semaphores.size(); ++i) {
        VkResult render_semaphore_result = vkCreateSemaphore(
            m_device,
            &semaphore_info,
            nullptr,
            &m_render_finished_semaphores[i]
        );

        logger.check(
            render_semaphore_result == VK_SUCCESS,
            "Failed to create per-swapchain-image render finished semaphore"
        );
    }
}

void VulkanEngine::draw_frame() {
    LOG_METHOD();

    vkWaitForFences(
        m_device,
        1,
        &m_in_flight_fences[m_current_frame],
        VK_TRUE,
        UINT64_MAX
    );

    uint32_t image_index = 0;

    VkResult acquire_result = vkAcquireNextImageKHR(
        m_device,
        m_swapchain,
        UINT64_MAX,
        m_image_available_semaphores[m_current_frame],
        VK_NULL_HANDLE,
        &image_index
    );

    logger.check(
        acquire_result == VK_SUCCESS,
        "Failed to acquire swapchain image"
    );

    vkResetFences(
        m_device,
        1,
        &m_in_flight_fences[m_current_frame]
    );

    vkResetCommandBuffer(
        m_commandBuffers[m_current_frame],
        0
    );

    record_command_buffer(
        m_commandBuffers[m_current_frame],
        image_index
    );

    VkSemaphore wait_semaphores[] = {
        m_image_available_semaphores[m_current_frame]
    };

    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };

    // Важно: render-finished semaphore берём по image_index,
    // а не по m_currentFrame.
    VkSemaphore signal_semaphores[] = {
        m_render_finished_semaphores[image_index]
    };

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_commandBuffers[m_current_frame];

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    VkResult submit_result = vkQueueSubmit(
        m_graphics_queue,
        1,
        &submit_info,
        m_in_flight_fences[m_current_frame]
    );

    logger.check(
        submit_result == VK_SUCCESS,
        "Failed to submit draw command buffer"
    );

    VkSwapchainKHR swapchains[] = {
        m_swapchain
    };

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;

    VkResult present_result = vkQueuePresentKHR(
        m_present_queue,
        &present_info
    );

    logger.check(
        present_result == VK_SUCCESS,
        "Failed to present swapchain image"
    );

    m_current_frame = (m_current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void VulkanEngine::record_command_buffer(
    VkCommandBuffer command_buffer,
    uint32_t image_index) 
{
    LOG_METHOD();

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkResult begin_result = vkBeginCommandBuffer(command_buffer, &begin_info);
    logger.check(begin_result == VK_SUCCESS, "Failed to begin recording command buffer");

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = m_render_pass;
    render_pass_info.framebuffer = m_swapchain_framebuffers[image_index];

    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = m_swapchainExtent;

    VkClearValue clear_color{};
    clear_color.color = {{0.05f, 0.08f, 0.12f, 1.0f}};

    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(
        command_buffer,
        &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE
    );

    vkCmdEndRenderPass(command_buffer);

    VkResult end_result = vkEndCommandBuffer(command_buffer);
    logger.check(end_result == VK_SUCCESS, "Failed to record command buffer");
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

    logger.check(glfwVulkanSupported(), "GLFW reports Vulkan is not supported on this machine");

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

    logger.check(glfw_extensions != nullptr, "glfwGetRequiredInstanceExtensions returned nullptr");
    logger.check(glfw_extension_count > 0, "GLFW returned zero required Vulkan extensions");

    std::vector<const char*> extensions(glfw_extensions, glfw_extensions + glfw_extension_count);

    if (m_enable_validation_layers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void VulkanEngine::create_instance() {
    LOG_METHOD();

    if (m_enable_validation_layers) {
        logger.check(
            check_validation_layer_support(),
            "Validation layers requested, but not available"
        );
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = m_window.title().c_str();
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

        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        populate_debug_messenger_create_info(debug_create_info);
        create_info.pNext = &debug_create_info;
    } else {
        create_info.enabledLayerCount = 0;
        create_info.ppEnabledLayerNames = nullptr;
    }

    VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
    logger.check(result == VK_SUCCESS, "Failed to create Vulkan instance");
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

    logger.check(result == VK_SUCCESS, "Failed to set up debug messenger");
}

void VulkanEngine::create_surface() {
    LOG_METHOD();

    VkResult result = glfwCreateWindowSurface(m_instance, m_window.handle(), nullptr, &m_surface);
    logger.check(result == VK_SUCCESS, "Failed to create window surface");
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanEngine::debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data)
{
    (void)message_type;
    (void)user_data;

    const char* severity_text = "INFO";
    uint32_t color = LoggerPalette::white;

    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severity_text = "ERROR";
        color = LoggerPalette::error;
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severity_text = "WARNING";
        color = LoggerPalette::yellow;
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        severity_text = "INFO";
        color = LoggerPalette::blue;
    } else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        severity_text = "VERBOSE";
        color = LoggerPalette::gray;
    }

    std::cerr << clr("[VULKAN ", color)
              << clr(severity_text, color)
              << clr("] ", color)
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


