#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>

#include "debug/DebugUI.hpp"
#include "core/VulkanContext.hpp"
#include "core/Swapchain.hpp"
#include "renderer/Renderer.hpp"

#include "imgui-docking/imgui.h"
#include "imgui-docking/backends/imgui_impl_glfw.h"
#include "imgui-docking/backends/imgui_impl_vulkan.h"

#include <cstdio>
#include <vector>
#include <string>

void DebugUI::init(VulkanContext& ctx, Swapchain& swapchain, GLFWwindow* window) {
    ImGui::CreateContext();

    // Ensure directory exists before ImGui tries to write the ini file
    CreateDirectoryA("config", nullptr);
    CreateDirectoryA("config\\imgui", nullptr);
    ImGui::GetIO().IniFilename = "config/imgui/imgui.ini";

    ImGui_ImplGlfw_InitForVulkan(window, true);

    VkFormat colorFormat = swapchain.format;

    VkPipelineRenderingCreateInfoKHR pipelineRenderingInfo{};
    pipelineRenderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    pipelineRenderingInfo.colorAttachmentCount = 1;
    pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = ctx.instance;
    initInfo.PhysicalDevice = ctx.physicalDevice;
    initInfo.Device = ctx.device;
    initInfo.QueueFamily = ctx.computeQueueFamily;
    initInfo.Queue = ctx.computeQueue;
    initInfo.DescriptorPoolSize = 32;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = (uint32_t)swapchain.images.size();
    initInfo.UseDynamicRendering = true;
    initInfo.PipelineInfoMain.PipelineRenderingCreateInfo = pipelineRenderingInfo;

    ImGui_ImplVulkan_Init(&initInfo);
}

void DebugUI::shutdown(VulkanContext& /*ctx*/) {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void DebugUI::draw(Renderer& renderer, const FrameStats& stats) {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Load JSON Scene file..."))
                openSceneFileDialog();
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    ImGui::SetNextWindowPos(ImVec2(10, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(320, 580), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Radiance Cascades Debug")) {
        if (ImGui::CollapsingHeader("Performance", ImGuiTreeNodeFlags_DefaultOpen))
            panelPerformance(renderer, stats);
        if (ImGui::CollapsingHeader("Visualization", ImGuiTreeNodeFlags_DefaultOpen))
            panelVisualization(renderer);
        if (ImGui::CollapsingHeader("Scene Controls"))
            panelSceneControls(renderer);
        if (ImGui::CollapsingHeader("Indirect Light Controls"))
            panelIndirectLightControls(renderer);
        if (ImGui::CollapsingHeader("Direct Light Controls"))
            panelDirectLightControls(renderer);
        if (ImGui::CollapsingHeader("VRAM"))
            panelVRAM(stats);
    }
    ImGui::End();

    ImGui::Render();
}

void DebugUI::renderOnSwapchain(VkCommandBuffer cmd, VkImageView swapchainView, VkExtent2D extent) {
    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = swapchainView;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = { {0, 0}, extent };
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    vkCmdBeginRendering(cmd, &renderingInfo);
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
    vkCmdEndRendering(cmd);
}

// ---- Panels ----

void DebugUI::panelPerformance(Renderer& renderer, const FrameStats& stats) {
    frameTimeHistory.push_back(stats.frameTimeMs);
    if ((int)frameTimeHistory.size() > kHistorySize)
        frameTimeHistory.pop_front();

    std::vector<float> history(frameTimeHistory.begin(), frameTimeHistory.end());
    char overlay[32];
    snprintf(overlay, sizeof(overlay), "%.1f ms", stats.frameTimeMs);
    ImGui::PlotLines("##ft", history.data(), (int)history.size(), 0, overlay, 0.0f, 50.0f, ImVec2(0, 60));

    float fps = stats.frameTimeMs > 0.0f ? 1000.0f / stats.frameTimeMs : 0.0f;
    ImGui::Text("CPU frame:  %.2f ms  (%.0f FPS)", stats.frameTimeMs, fps);
    ImGui::Separator();
    ImGui::Text("GPU total:  %.2f ms", stats.gpuTotalMs);
    if (!renderer.useLegacyPipeline) {
        ImGui::Text("  Primary:  %.2f ms", stats.gpuPrimaryMs);
        ImGui::Text("  Alloc:    %.2f ms", stats.gpuAllocMs);
        ImGui::Text("  Trace:    %.2f ms", stats.gpuTraceMs);
        ImGui::Text("  Merge:    %.2f ms", stats.gpuMergeMs);
        ImGui::Text("  Gather:   %.2f ms", stats.gpuGatherMs);
        ImGui::Text("  Transp:   %.2f ms", stats.gpuTransparentMs);
        ImGui::Text("  Tonemap:  %.2f ms", stats.gpuTonemapMs);
    }
}

void DebugUI::panelVisualization(Renderer& renderer) {
    bool legacyMode = renderer.useLegacyPipeline;
    if (ImGui::Checkbox("Use legacy pipeline", &legacyMode)) {
        pendingLegacyToggle = legacyMode ? 1 : 0;
    }
    ImGui::Separator();

    bool legacy = renderer.useLegacyPipeline;

    if (legacy) {
        ImGui::TextDisabled("Not used in legacy mode.");
        ImGui::BeginDisabled();
    }

    const char* modes[] = { "Final", "Albedo", "Normal", "Depth", "Emissive", "Indirect Only", "Probes" };
    ImGui::Combo("Debug mode", &renderer.debugMode, modes, IM_ARRAYSIZE(modes));

    if (renderer.debugMode == 6) {
        ImGui::SeparatorText("Probe visualization");
        int maxLvl = renderer.rcConfig.numCascades - 1;
        ImGui::SliderInt("Cascade level", &renderer.probeVizLevel, 0, maxLvl);
        ImGui::SliderFloat("Body radius", &renderer.probeVizRadius, 0.02f, 0.5f, "%.3f wu");
        ImGui::SliderFloat("Ray length", &renderer.probeVizRayLen, 0.05f, 3.0f, "%.3f wu");
        ImGui::SliderFloat("Ray radius", &renderer.probeVizRayRad, 0.003f, 0.1f, "%.3f wu");
        ImGui::Separator();
    }

    bool direct = renderer.enableDirect != 0;
    bool indirect = renderer.enableIndirect != 0;
    if (ImGui::Checkbox("Enable direct", &direct))   renderer.enableDirect = direct ? 1 : 0;
    if (ImGui::Checkbox("Enable indirect", &indirect)) renderer.enableIndirect = indirect ? 1 : 0;

    ImGui::SliderFloat("Exposure", &renderer.exposure, 0.1f, 5.0f);

    const char* tonemappers[] = { "ACES Film", "Reinhard", "Linear" };
    ImGui::Combo("Tonemapper", &renderer.tonemapMode, tonemappers, IM_ARRAYSIZE(tonemappers));

    if (legacy) ImGui::EndDisabled();
}

void DebugUI::panelSceneControls(Renderer& renderer) {
    bool legacy = renderer.useLegacyPipeline;

    ImGui::SeparatorText("Camera Settings");
    ImGui::SliderFloat("Focal dist", &renderer.focalDistance, 0.1f, 20.0f);
    ImGui::SliderFloat("Lens radius", &renderer.lensRadius, 0.0f, 0.5f);

    ImGui::SeparatorText("Atmosphere");
    ImGui::SliderFloat("Fog Density", &renderer.fogDensity, 0.0f, 0.1f, "%.3f");

    bool fog = renderer.enableFog != 0;
    if (ImGui::Checkbox("Fog", &fog)) renderer.enableFog = fog ? 1 : 0;
    if (fog) {
        ImGui::ColorEdit3("Fog color", &renderer.fogColor.x);
        if (legacy) ImGui::BeginDisabled();
        ImGui::SameLine();
        ImGui::Checkbox("Blends with sky", &renderer.fogBlendWithSky);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Not implemented yet, waiting for future integration.");
        if (legacy) ImGui::EndDisabled();
    }

    ImGui::Separator();
    bool sky = renderer.enableSkybox != 0;
    if (ImGui::Checkbox("Skybox", &sky)) renderer.enableSkybox = sky ? 1 : 0;
    if (sky) {
        ImGui::ColorEdit3("Sky bottom", &renderer.skyBottomColor.x);
        ImGui::ColorEdit3("Sky top", &renderer.skyTopColor.x);
    }

    ImGui::SeparatorText("Materials");
    bool tex = renderer.enableTextures != 0;
    if (ImGui::Checkbox("Textures", &tex)) renderer.enableTextures = tex ? 1 : 0;
}

void DebugUI::panelIndirectLightControls(Renderer& renderer) {
    if (renderer.useLegacyPipeline) {
        ImGui::TextDisabled("Not used in legacy mode.");
        ImGui::BeginDisabled();
    }

    ImGui::SliderFloat("Indirect Scale", &renderer.kIndirectScale, 0.0f, 0.1f, "%.4f");
    ImGui::SliderFloat("2nd Bounce Weight", &renderer.bounceWeight, 0.0f, 1.0f, "%.2f");
    ImGui::SliderFloat("Bounce EMA Alpha", &renderer.bounceEmaAlpha, 0.01f, 0.5f, "%.2f");
    ImGui::Separator();

    if (pendingNumCascades == 0)
        pendingNumCascades = renderer.rcConfig.numCascades;

    ImGui::SliderInt("Cascades", &pendingNumCascades, 1, 8);
    ImGui::SliderInt("Branching factor", &renderer.rcConfig.branchingFactor, 1, 4);
    ImGui::SliderFloat("Spacing 0", &renderer.rcConfig.spacing0, 0.1f, 5.0f);
    ImGui::SliderInt("Oct Res 0", &renderer.rcConfig.octRes0, 1, 16);
    ImGui::SliderInt("Probe shadow rays", &renderer.probeSoftShadowSamples, 1, 8);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("1 = hard shadow (original). >1 = area-light jitter baked into probe irradiance.\nOnly affects cascade-0. Revert to 1 to restore original behavior.");

    if (ImGui::Button("Apply")) {
        renderer.rcConfig.numCascades = pendingNumCascades;
        renderer.recreateCascades();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(changes take effect on Apply)");

    if (renderer.useLegacyPipeline)
        ImGui::EndDisabled();
}

void DebugUI::panelDirectLightControls(Renderer& renderer) {
    ImGui::SeparatorText("Ray Bounces");
    ImGui::SliderInt("Max depth", &renderer.maxDepth, 1, 16);
    ImGui::SliderInt("Primary rays", &renderer.primaryRaysPerPixel, 1, 8);

    ImGui::SeparatorText("Shadows");
    ImGui::SliderInt("Shadow rays", &renderer.shadowRays, 0, 16);

    ImGui::SeparatorText("Caustics");
    bool useCaustics = renderer.enableCaustics != 0;
    if (ImGui::Checkbox("Enable Caustics", &useCaustics)) renderer.enableCaustics = useCaustics ? 1 : 0;
    if (useCaustics) {
        int photons = (int)renderer.totalEmittedPhotons;
        if (ImGui::DragInt("Photon Count", &photons, 10000, 10000, 5000000)) {
            renderer.totalEmittedPhotons = (uint32_t)photons;
        }
        ImGui::SliderFloat("Intensity", &renderer.causticIntensity, 0.0f, 50.0f);
    }
}

void DebugUI::panelVRAM(const FrameStats& stats) {
    float usedMB = (float)(stats.vramUsedBytes / (1024 * 1024));
    float budgetMB = (float)(stats.vramBudgetBytes / (1024 * 1024));
    float fraction = (budgetMB > 0.0f) ? (usedMB / budgetMB) : 0.0f;

    char label[64];
    snprintf(label, sizeof(label), "%.0f / %.0f MB", usedMB, budgetMB);
    ImGui::ProgressBar(fraction, ImVec2(-1.0f, 0.0f), label);
}

void DebugUI::openSceneFileDialog() {
    wchar_t buf[MAX_PATH] = {};
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = L"JSON Files\0*.json\0All Files\0*.*\0";
    ofn.lpstrFile = buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetOpenFileNameW(&ofn)) {
        int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, nullptr, 0, nullptr, nullptr);
        if (len > 0) {
            std::string result(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, buf, -1, result.data(), len, nullptr, nullptr);
            pendingScenePath = std::move(result);
        }
    }
}