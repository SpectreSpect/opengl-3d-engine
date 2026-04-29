#include "vulkan_engine.h"

VulkanEngine::VulkanEngine(const GlfwContext& glfw_context, Window& window, std::string_view app_name) 
    :   m_window(window), 
        m_instance(glfw_context, app_name),
        m_surface(m_instance, m_window),
        m_physical_device(m_instance, m_surface)
{
    
}

VulkanEngine::~VulkanEngine() {
    destroy();
}

void VulkanEngine::destroy() {
    LOG_METHOD();

    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);

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

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
}

void VulkanEngine::init() {
    LOG_METHOD();

    init_vulkan();
}

void VulkanEngine::init_vulkan() {
    LOG_METHOD();

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

void VulkanEngine::create_logical_device() {
    LOG_METHOD();

    QueueFamilyIndices indices = m_physical_device.find_queue_families();

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

    create_info.enabledExtensionCount = static_cast<uint32_t>(VulkanPhysicalDevice::device_extensions.size());
    create_info.ppEnabledExtensionNames = VulkanPhysicalDevice::device_extensions.data();

    create_info.enabledLayerCount = 0;
    create_info.ppEnabledLayerNames = nullptr;

    VkResult result = vkCreateDevice(m_physical_device.handle(), &create_info, nullptr, &m_device);
    logger.check(result == VK_SUCCESS, "Failed to create logical device");

    vkGetDeviceQueue(m_device, indices.graphics_family.value(), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, indices.present_family.value(), 0, &m_present_queue);
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

    auto swapchain_support = m_physical_device.query_swapchain_support();
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
    create_info.surface = m_surface.handle();

    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = m_physical_device.find_queue_families();
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

    QueueFamilyIndices queue_family_indices = m_physical_device.find_queue_families();

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
