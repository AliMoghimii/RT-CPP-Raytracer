#include "core/CommandManager.hpp"
#include "core/VulkanContext.hpp"
#include <stdexcept>

void CommandManager::create(VulkanContext& ctx, uint32_t swapchainImageCount) {
    VkCommandPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pi.queueFamilyIndex = ctx.computeQueueFamily;

    if (vkCreateCommandPool(ctx.device, &pi, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("CommandManager: command pool creation failed.");

    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(ctx.device, &ai, &buffer) != VK_SUCCESS)
        throw std::runtime_error("CommandManager: command buffer allocation failed.");

    VkSemaphoreCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fi{};
    fi.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(ctx.device, &si, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateFence(ctx.device, &fi, nullptr, &inFlightFence) != VK_SUCCESS)
        throw std::runtime_error("CommandManager: sync object creation failed.");

    renderFinishedSemaphores.resize(swapchainImageCount);
    for (uint32_t i = 0; i < swapchainImageCount; i++) {
        if (vkCreateSemaphore(ctx.device, &si, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS)
            throw std::runtime_error("CommandManager: renderFinished semaphore creation failed.");
    }
}

void CommandManager::destroy(VulkanContext& ctx) {
    vkDestroyFence(ctx.device, inFlightFence, nullptr);
    for (auto sem : renderFinishedSemaphores)
        vkDestroySemaphore(ctx.device, sem, nullptr);
    vkDestroySemaphore(ctx.device, imageAvailableSemaphore, nullptr);
    vkDestroyCommandPool(ctx.device, pool, nullptr);
}

VkCommandBuffer CommandManager::beginOneTime(VulkanContext& ctx) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(ctx.device, &ai, &cmd);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &bi);
    return cmd;
}

void CommandManager::submitOneTime(VulkanContext& ctx, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.computeQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.computeQueue);
    vkFreeCommandBuffers(ctx.device, pool, 1, &cmd);
}
