#include "imgui_layer.h"

#include <cassert>

#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include "vulkan_engine.h"
#include "vulkan_window.h"

namespace ui {

void init(GLFWwindow* window, const VulkanInitInfo& info) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    const bool glfw_ok = ImGui_ImplGlfw_InitForVulkan(window, true);
    assert(glfw_ok && "ImGui_ImplGlfw_InitForVulkan failed");

    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = info.instance;
    init_info.PhysicalDevice = info.physical_device;
    init_info.Device = info.device;
    init_info.QueueFamily = info.queue_family;
    init_info.Queue = info.queue;
    init_info.DescriptorPool = info.descriptor_pool;
    init_info.RenderPass = info.render_pass;
    init_info.MinImageCount = info.min_image_count;
    init_info.ImageCount = info.image_count;
    init_info.MSAASamples = info.msaa_samples;
    init_info.PipelineCache = info.pipeline_cache;
    init_info.Subpass = info.subpass;
    init_info.Allocator = info.allocator;
    init_info.CheckVkResultFn = info.check_vk_result_fn;

    const bool vulkan_ok = ImGui_ImplVulkan_Init(&init_info);
    assert(vulkan_ok && "ImGui_ImplVulkan_Init failed");
}


void init(VulkanWindow* window, VulkanEngine* engine) {
    ui::VulkanInitInfo ui_info{};
    ui_info.instance = engine->instance;
    ui_info.physical_device = engine->physicalDevice;
    ui_info.device = engine->device;
    ui_info.queue_family = engine->get_graphics_queue_family();
    ui_info.queue = engine->graphicsQueue;
    ui_info.descriptor_pool = engine->get_imgui_descriptor_pool();
    ui_info.render_pass = engine->renderPass;
    ui_info.min_image_count = engine->get_imgui_min_image_count();
    ui_info.image_count = static_cast<uint32_t>(engine->swapchainImages.size());
    ui_info.msaa_samples = VK_SAMPLE_COUNT_1_BIT;
    ui_info.pipeline_cache = VK_NULL_HANDLE;
    ui_info.subpass = 0;
    ui_info.check_vk_result_fn = VulkanEngine::imgui_check_vk_result;

    ui::init(window->window, ui_info);
}

void begin_frame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void update_mouse_mode(VulkanWindow* window) {
    ImGuiIO& io = ImGui::GetIO();

    if (window->mouse_state.mode == MouseMode::NORMAL)
        io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    else
        io.ConfigFlags |= ImGuiConfigFlags_NoMouse;
}

void end_frame(VkCommandBuffer command_buffer) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
}

void set_min_image_count(uint32_t min_image_count) {
    ImGui_ImplVulkan_SetMinImageCount(min_image_count);
}

void shutdown() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}





} // namespace ui

