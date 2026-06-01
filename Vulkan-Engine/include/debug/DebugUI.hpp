#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <deque>
#include <string>

#include "debug/FrameStats.hpp"

class VulkanContext;
class Swapchain;
class Renderer;

class DebugUI {
public:
    std::string pendingScenePath; // set by openSceneFileDialog(); consumed by Renderer when loading is implemented

    void init(VulkanContext& ctx, Swapchain& swapchain, GLFWwindow* window);
    void shutdown(VulkanContext& ctx);

    // Called once per frame before recordCommandBuffer: builds the full ImGui draw list.
    void draw(Renderer& renderer, const FrameStats& stats);

    // Called from inside recordCommandBuffer after the ldrImage -> swapchain copy.
    // Swapchain image must be in COLOR_ATTACHMENT_OPTIMAL layout on entry and exit.
    void renderOnSwapchain(VkCommandBuffer cmd, VkImageView swapchainView, VkExtent2D extent);

private:
    static constexpr int kHistorySize = 240;
    std::deque<float> frameTimeHistory;

    void panelPerformance(Renderer& renderer, const FrameStats& stats);
    void panelCascadeConfig(Renderer& renderer);
    void panelVisualization(Renderer& renderer);
    void panelSceneControls(Renderer& renderer);
    void panelLegacy(Renderer& renderer);
    void panelVRAM(const FrameStats& stats);
    void openSceneFileDialog();
};
