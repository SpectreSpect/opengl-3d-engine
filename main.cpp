#include "vulkan_self/vulkan_engine.h"

int main() {
    GlfwContext glfw_context;
    Window window(glfw_context, 1280, 720, "Vulkan engine");
    
    VulkanEngine engine(glfw_context, window);
    engine.init();
    engine.run();
}
