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
    int pendingLegacyToggle = -1; // -1 = none, 0/1 = requested useLegacyPipeline value; consumed by Renderer::mainLoop

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
    int pendingNumCascades = 0;  // 0 = uninitialized; staged separately from rcConfig to avoid mid-frame OOB

    void panelPerformance(Renderer& renderer, const FrameStats& stats);
    void panelVisualization(Renderer& renderer);
    void panelSceneControls(Renderer& renderer);
    void panelIndirectLightControls(Renderer& renderer);
    void panelDirectLightControls(Renderer& renderer);
    void panelVRAM(const FrameStats& stats);
    void openSceneFileDialog();
};