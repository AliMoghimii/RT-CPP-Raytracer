#include "core/VulkanContext.hpp"
#include <stdexcept>

void VulkanContext::initialize(GLFWwindow* window) {
    vkb::InstanceBuilder builder;
    auto instResult = builder
        .set_app_name("RC Engine")
        .request_validation_layers(true)
        .require_api_version(1, 3, 0)
        .use_default_debug_messenger()
        .build();
    if (!instResult)
        throw std::runtime_error("VulkanContext: instance creation failed — " + instResult.error().message());

    auto vkbInst = instResult.value();
    instance = vkbInst.instance;
    debugMessenger = vkbInst.debug_messenger;

    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
        throw std::runtime_error("VulkanContext: window surface creation failed.");

    VkPhysicalDeviceVulkan13Features v13{};
    v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    v13.synchronization2 = VK_TRUE;
    v13.dynamicRendering = VK_TRUE;

    VkPhysicalDeviceVulkan12Features v12{};
    v12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    v12.runtimeDescriptorArray = VK_TRUE;
    v12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

    VkPhysicalDeviceFeatures baseFeatures{};
    baseFeatures.samplerAnisotropy = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{ vkbInst };
    auto physResult = selector
        .set_surface(surface)
        .set_minimum_version(1, 3)
        .set_required_features_13(v13)
        .set_required_features_12(v12)
        .set_required_features(baseFeatures)
        .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
        .select();
    if (!physResult)
        throw std::runtime_error("VulkanContext: physical device selection failed — " + physResult.error().message());

    physicalDevice = physResult.value().physical_device;

    vkb::DeviceBuilder devBuilder{ physResult.value() };
    auto devResult = devBuilder.build();
    if (!devResult)
        throw std::runtime_error("VulkanContext: logical device creation failed — " + devResult.error().message());

    device = devResult.value().device;

    // Use graphics queue: it supports compute + present on all desktop GPUs.
    // QueueType::compute can return a dedicated async-compute queue (no present) on AMD.
    auto qResult = devResult.value().get_queue_and_index(vkb::QueueType::graphics);
    if (!qResult)
        throw std::runtime_error("VulkanContext: queue retrieval failed.");
    computeQueue = qResult.value().first;
    computeQueueFamily = qResult.value().second;

    VmaAllocatorCreateInfo aci{};
    aci.physicalDevice = physicalDevice;
    aci.device = device;
    aci.instance = instance;
    aci.vulkanApiVersion = VK_API_VERSION_1_3;
    if (vmaCreateAllocator(&aci, &allocator) != VK_SUCCESS)
        throw std::runtime_error("VulkanContext: VMA allocator creation failed.");
}

void VulkanContext::shutdown() {
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debugMessenger);
    vkDestroyInstance(instance, nullptr);
}
