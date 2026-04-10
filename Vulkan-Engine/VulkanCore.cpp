#include "VulkanCore.hpp"
#include <stdexcept>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

using namespace std;

void VulkanCore::run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}

void VulkanCore::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(1280, 720, "Vulkan Real-Time Raytracer", nullptr, nullptr);
}

void VulkanCore::initVulkan() {
    createInstance();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createCommandPool();
    createSwapchain();
    createSwapchainImageViews();
    createComputeImage();
    createSceneBuffers();
    createDescriptorSetLayout();
    createComputePipeline();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

void VulkanCore::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "RTRT Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "RTRT Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_2;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;

    if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
        throw runtime_error("Failed to create Vulkan instance.");
    }
}

void VulkanCore::createSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        throw runtime_error("Failed to create window surface.");
    }
}

void VulkanCore::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        throw runtime_error("Failed to find GPUs with Vulkan support.");
    }

    vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    int bestScore = -1;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

    for (const auto& dev : devices) {
        int score = ratePhysicalDevice(dev);
        if (score > bestScore) {
            bestScore = score;
            bestDevice = dev;
        }
    }

    if (bestDevice == VK_NULL_HANDLE || bestScore < 0) {
        throw runtime_error("Failed to find a suitable GPU.");
    }

    physicalDevice = bestDevice;

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice, &props);
    cout << "Selected GPU: " << props.deviceName << " (score: " << bestScore << ")\n";
}

int VulkanCore::ratePhysicalDevice(VkPhysicalDevice dev) {
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceProperties(dev, &props);
    vkGetPhysicalDeviceFeatures(dev, &features);

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
    vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, queueFamilies.data());

    bool hasComputeAndPresent = false;
    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
        if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && presentSupport) {
            hasComputeAndPresent = true;
            break;
        }
    }
    if (!hasComputeAndPresent) return -1;

    uint32_t extCount = 0;
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
    vector<VkExtensionProperties> extensions(extCount);
    vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, extensions.data());

    bool hasSwapchain = false;
    for (const auto& ext : extensions) {
        if (strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0) {
            hasSwapchain = true;
            break;
        }
    }
    if (!hasSwapchain) return -1;

    int score = 0;

    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
        score += 10000;
    else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
        score += 1000;

    score += props.limits.maxImageDimension2D;

    return score;
}

void VulkanCore::createLogicalDevice() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    int computeFamily = -1;
    for (int i = 0; i < queueFamilies.size(); i++) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

        if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && presentSupport) {
            computeFamily = i;
            break;
        }
    }

    if (computeFamily == -1) {
        throw runtime_error("Failed to find a compute queue family.");
    }

    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = computeFamily;
    queueCreateInfo.queueCount = 1;
    float queuePriority = 1.0f;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    const vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.queueCreateInfoCount = 1;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
        throw runtime_error("Failed to create logical device.");
    }

    vkGetDeviceQueue(device, computeFamily, 0, &computeQueue);
}

void VulkanCore::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = 0;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
        throw runtime_error("Failed to create command pool.");
    }
}

void VulkanCore::createSwapchain() {
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = 2;
    createInfo.imageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = { 1280, 720 };
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw runtime_error("Failed to create swap chain.");
    }

    uint32_t imageCount;
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

    swapChainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    swapChainExtent = { 1280, 720 };
}

void VulkanCore::createSwapchainImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

    for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
            throw runtime_error("Failed to create image views.");
        }
    }
}

void VulkanCore::createComputeImage() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = 1280;
    imageInfo.extent.height = 720;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    if (vkCreateImage(device, &imageInfo, nullptr, &computeImage) != VK_SUCCESS) {
        throw runtime_error("Failed to create compute image.");
    }

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, computeImage, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &computeImageMemory) != VK_SUCCESS) {
        throw runtime_error("Failed to allocate compute image memory.");
    }

    vkBindImageMemory(device, computeImage, computeImageMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = computeImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &computeImageView) != VK_SUCCESS) {
        throw runtime_error("Failed to create compute image view.");
    }

    VkCommandBufferAllocateInfo allocInfoCmd{};
    allocInfoCmd.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfoCmd.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfoCmd.commandPool = commandPool;
    allocInfoCmd.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfoCmd, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &beginInfo);
    transitionImageLayout(cmd, computeImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    vkQueueSubmit(computeQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(computeQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

void VulkanCore::loadScene(
    const vector<GPUMaterial>& mats,
    const vector<GPUSphere>& sphs,
    const vector<GPUTriangle>& tris,
    const vector<GPULight>& lghts,
    const vector<GPUPlane>& plns,
    const vector<GPUQuad>& quds,
    const vector<GPUCube>& cbs,
    const vector<GPUBVHNode>& bvh
) {
    sceneMaterials = mats;
    sceneSpheres = sphs;
    sceneTriangles = tris;
    sceneLights = lghts;
    scenePlanes = plns;
    sceneQuads = quds;
    sceneCubes = cbs;
    sceneBVH = bvh;
}

void VulkanCore::createSceneBuffers() {
    VkDeviceSize matBufferSize = sizeof(GPUMaterial) * max((size_t)1, sceneMaterials.size());
    VkDeviceSize sphBufferSize = sizeof(GPUSphere) * max((size_t)1, sceneSpheres.size());
    VkDeviceSize triBufferSize = sizeof(GPUTriangle) * max((size_t)1, sceneTriangles.size());
    VkDeviceSize lgtBufferSize = sizeof(GPULight) * max((size_t)1, sceneLights.size());
    VkDeviceSize plnBufferSize = sizeof(GPUPlane) * max((size_t)1, scenePlanes.size());
    VkDeviceSize qudBufferSize = sizeof(GPUQuad) * max((size_t)1, sceneQuads.size());
    VkDeviceSize cubBufferSize = sizeof(GPUCube) * max((size_t)1, sceneCubes.size());
    VkDeviceSize bvhBufferSize = sizeof(GPUBVHNode) * max((size_t)1, sceneBVH.size());

    createBuffer(matBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, materialBuffer, materialMemory);
    createBuffer(sphBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sphereBuffer, sphereMemory);
    createBuffer(triBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, triangleBuffer, triangleMemory);
    createBuffer(lgtBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, lightBuffer, lightMemory);
    createBuffer(plnBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, planeBuffer, planeMemory);
    createBuffer(qudBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, quadBuffer, quadMemory);
    createBuffer(cubBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, cubeBuffer, cubeMemory);
    createBuffer(bvhBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, bvhBuffer, bvhMemory);

    void* data;

    if (!sceneMaterials.empty()) {
        vkMapMemory(device, materialMemory, 0, matBufferSize, 0, &data);
        memcpy(data, sceneMaterials.data(), (size_t)matBufferSize);
        vkUnmapMemory(device, materialMemory);
    }
    if (!sceneSpheres.empty()) {
        vkMapMemory(device, sphereMemory, 0, sphBufferSize, 0, &data);
        memcpy(data, sceneSpheres.data(), (size_t)sphBufferSize);
        vkUnmapMemory(device, sphereMemory);
    }
    if (!sceneTriangles.empty()) {
        vkMapMemory(device, triangleMemory, 0, triBufferSize, 0, &data);
        memcpy(data, sceneTriangles.data(), (size_t)triBufferSize);
        vkUnmapMemory(device, triangleMemory);
    }
    if (!sceneLights.empty()) {
        vkMapMemory(device, lightMemory, 0, lgtBufferSize, 0, &data);
        memcpy(data, sceneLights.data(), (size_t)lgtBufferSize);
        vkUnmapMemory(device, lightMemory);
    }
    if (!scenePlanes.empty()) {
        vkMapMemory(device, planeMemory, 0, plnBufferSize, 0, &data);
        memcpy(data, scenePlanes.data(), (size_t)plnBufferSize);
        vkUnmapMemory(device, planeMemory);
    }
    if (!sceneQuads.empty()) {
        vkMapMemory(device, quadMemory, 0, qudBufferSize, 0, &data);
        memcpy(data, sceneQuads.data(), (size_t)qudBufferSize);
        vkUnmapMemory(device, quadMemory);
    }
    if (!sceneCubes.empty()) {
        vkMapMemory(device, cubeMemory, 0, cubBufferSize, 0, &data);
        memcpy(data, sceneCubes.data(), (size_t)cubBufferSize);
        vkUnmapMemory(device, cubeMemory);
    }
    if (!sceneBVH.empty()) {
        vkMapMemory(device, bvhMemory, 0, bvhBufferSize, 0, &data);
        memcpy(data, sceneBVH.data(), (size_t)bvhBufferSize);
        vkUnmapMemory(device, bvhMemory);
    }
}

void VulkanCore::createDescriptorSetLayout() {
    vector<VkDescriptorSetLayoutBinding> bindings(9);

    for (int i = 0; i < 9; i++) {
        bindings[i].binding = i;
        if (i == 0) {
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        }
        else {
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS) {
        throw runtime_error("Failed to create descriptor set layout.");
    }
}

void VulkanCore::createComputePipeline() {
    auto compShaderCode = readFile("shaders/raytracer.comp.spv");
    VkShaderModule compShaderModule = createShaderModule(compShaderCode);

    VkPipelineShaderStageCreateInfo shaderStageInfo{};
    shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shaderStageInfo.module = compShaderModule;
    shaderStageInfo.pName = "main";

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(CameraPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
        throw runtime_error("Failed to create layout.");
    }

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.stage = shaderStageInfo;

    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
        throw runtime_error("Failed to create pipeline.");
    }

    vkDestroyShaderModule(device, compShaderModule, nullptr);
}

void VulkanCore::createDescriptorPool() {
    vector<VkDescriptorPoolSize> poolSizes(2);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = 1;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = 8;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS) {
        throw runtime_error("Failed to create pool.");
    }
}

void VulkanCore::createDescriptorSets() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS) {
        throw runtime_error("Failed to allocate descriptor sets.");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    imageInfo.imageView = computeImageView;

    VkDescriptorBufferInfo matBufferInfo{};
    matBufferInfo.buffer = materialBuffer;
    matBufferInfo.offset = 0;
    matBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo sphBufferInfo{};
    sphBufferInfo.buffer = sphereBuffer;
    sphBufferInfo.offset = 0;
    sphBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo triBufferInfo{};
    triBufferInfo.buffer = triangleBuffer;
    triBufferInfo.offset = 0;
    triBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo lgtBufferInfo{};
    lgtBufferInfo.buffer = lightBuffer;
    lgtBufferInfo.offset = 0;
    lgtBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo plnBufferInfo{};
    plnBufferInfo.buffer = planeBuffer;
    plnBufferInfo.offset = 0;
    plnBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo qudBufferInfo{};
    qudBufferInfo.buffer = quadBuffer;
    qudBufferInfo.offset = 0;
    qudBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo cubBufferInfo{};
    cubBufferInfo.buffer = cubeBuffer;
    cubBufferInfo.offset = 0;
    cubBufferInfo.range = VK_WHOLE_SIZE;

    VkDescriptorBufferInfo bvhBufferInfo{};
    bvhBufferInfo.buffer = bvhBuffer;
    bvhBufferInfo.offset = 0;
    bvhBufferInfo.range = VK_WHOLE_SIZE;

    vector<VkWriteDescriptorSet> descriptorWrites(9);

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = descriptorSet;
    descriptorWrites[0].dstBinding = 0;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &imageInfo;

    descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[1].dstSet = descriptorSet;
    descriptorWrites[1].dstBinding = 1;
    descriptorWrites[1].dstArrayElement = 0;
    descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[1].descriptorCount = 1;
    descriptorWrites[1].pBufferInfo = &matBufferInfo;

    descriptorWrites[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[2].dstSet = descriptorSet;
    descriptorWrites[2].dstBinding = 2;
    descriptorWrites[2].dstArrayElement = 0;
    descriptorWrites[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[2].descriptorCount = 1;
    descriptorWrites[2].pBufferInfo = &sphBufferInfo;

    descriptorWrites[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[3].dstSet = descriptorSet;
    descriptorWrites[3].dstBinding = 3;
    descriptorWrites[3].dstArrayElement = 0;
    descriptorWrites[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[3].descriptorCount = 1;
    descriptorWrites[3].pBufferInfo = &triBufferInfo;

    descriptorWrites[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[4].dstSet = descriptorSet;
    descriptorWrites[4].dstBinding = 4;
    descriptorWrites[4].dstArrayElement = 0;
    descriptorWrites[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[4].descriptorCount = 1;
    descriptorWrites[4].pBufferInfo = &lgtBufferInfo;

    descriptorWrites[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[5].dstSet = descriptorSet;
    descriptorWrites[5].dstBinding = 5;
    descriptorWrites[5].dstArrayElement = 0;
    descriptorWrites[5].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[5].descriptorCount = 1;
    descriptorWrites[5].pBufferInfo = &plnBufferInfo;

    descriptorWrites[6].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[6].dstSet = descriptorSet;
    descriptorWrites[6].dstBinding = 6;
    descriptorWrites[6].dstArrayElement = 0;
    descriptorWrites[6].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[6].descriptorCount = 1;
    descriptorWrites[6].pBufferInfo = &qudBufferInfo;

    descriptorWrites[7].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[7].dstSet = descriptorSet;
    descriptorWrites[7].dstBinding = 7;
    descriptorWrites[7].dstArrayElement = 0;
    descriptorWrites[7].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[7].descriptorCount = 1;
    descriptorWrites[7].pBufferInfo = &cubBufferInfo;

    descriptorWrites[8].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[8].dstSet = descriptorSet;
    descriptorWrites[8].dstBinding = 8;
    descriptorWrites[8].dstArrayElement = 0;
    descriptorWrites[8].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrites[8].descriptorCount = 1;
    descriptorWrites[8].pBufferInfo = &bvhBufferInfo;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanCore::createCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        throw runtime_error("Failed to allocate command buffers.");
    }
}

void VulkanCore::createSyncObjects() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        throw runtime_error("Failed to create synchronization objects.");
    }
}

void VulkanCore::processInput() {
    float cameraSpeed = 3.5f * deltaTime;
    float rotSpeed = 90.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        cameraPos += cameraSpeed * cameraUp;
    }
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * cameraUp;
    }

    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        yaw += rotSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        yaw -= rotSpeed;
    }

    glm::vec3 right = glm::normalize(glm::cross(cameraUp, cameraFront));

    glm::vec3 flatFront = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        cameraPos += cameraSpeed * flatFront;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        cameraPos -= cameraSpeed * flatFront;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        cameraPos -= right * cameraSpeed;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        cameraPos += right * cameraSpeed;
    }

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void VulkanCore::updateDynamicData() {
    if (sceneSpheres.size() > 4) {
        glm::vec3 eyeRCenter = sceneSpheres[1].center;
        glm::vec3 dirR = cameraPos - eyeRCenter;
        dirR = glm::normalize(dirR);
        sceneSpheres[2].center = eyeRCenter + (dirR * 0.09f);

        glm::vec3 eyeLCenter = sceneSpheres[3].center;
        glm::vec3 dirL = cameraPos - eyeLCenter;
        dirL = glm::normalize(dirL);
        sceneSpheres[4].center = eyeLCenter + (dirL * 0.09f);

        void* data;
        VkDeviceSize sphBufferSize = sizeof(GPUSphere) * sceneSpheres.size();

        vkMapMemory(device, sphereMemory, 0, sphBufferSize, 0, &data);
        memcpy(data, sceneSpheres.data(), (size_t)sphBufferSize);
        vkUnmapMemory(device, sphereMemory);
    }
}

void VulkanCore::mainLoop() {
    cout << "Rendering Started.\n";
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();
        processInput();
        updateDynamicData();
        drawFrame();
    }
    vkDeviceWaitIdle(device);
}

void VulkanCore::drawFrame() {
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFence);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

    vkResetCommandBuffer(commandBuffer, 0);
    recordCommandBuffer(commandBuffer, imageIndex);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };

    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };

    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(computeQueue, 1, &submitInfo, inFlightFence);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = { swapChain };

    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(computeQueue, &presentInfo);
}

void VulkanCore::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    CameraPushConstants pc{};
    pc.camPos = glm::vec4(cameraPos, 0.0f);
    pc.camForward = glm::vec4(cameraFront, 0.0f);
    pc.camRight = glm::vec4(glm::normalize(glm::cross(cameraUp, cameraFront)), 0.0f);
    pc.camUp = glm::vec4(glm::normalize(glm::cross(cameraFront, glm::vec3(pc.camRight))), 0.0f);
    pc.sphereCount = static_cast<int>(sceneSpheres.size());
    pc.triangleCount = static_cast<int>(sceneTriangles.size());
    pc.planeCount = static_cast<int>(scenePlanes.size());
    pc.quadCount = static_cast<int>(sceneQuads.size());
    pc.cubeCount = static_cast<int>(sceneCubes.size());
    pc.lightCount = static_cast<int>(sceneLights.size());
    pc.bvhCount = static_cast<int>(sceneBVH.size());
    pc.maxDepth = maxDepth;
    pc.shadowRays = shadowRays;
    pc.samplesPerPixel = samplesPerPixel;
    pc.focalDistance = focalDistance;
    pc.lensRadius = lensRadius;
    pc.enableFog = enableFog;
    pc.fogR = fogR;
    pc.fogG = fogG;
    pc.fogB = fogB;
    pc.enableSkybox = enableSkybox;
    pc.skyBottomR = skyBottomR;
    pc.skyBottomG = skyBottomG;
    pc.skyBottomB = skyBottomB;
    pc.skyTopR = skyTopR;
    pc.skyTopG = skyTopG;
    pc.skyTopB = skyTopB;
    pc.pad4 = 0.0f;

    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CameraPushConstants), &pc);

    vkCmdDispatch(cmd, 1280 / 16, 720 / 16, 1);

    transitionImageLayout(cmd, computeImage, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transitionImageLayout(cmd, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.extent.width = 1280;
    copyRegion.extent.height = 720;
    copyRegion.extent.depth = 1;

    vkCmdCopyImage(cmd, computeImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    transitionImageLayout(cmd, computeImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
    transitionImageLayout(cmd, swapChainImages[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(cmd);
}

void VulkanCore::transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else {
        throw invalid_argument("Unsupported layout transition.");
    }

    vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

vector<char> VulkanCore::readFile(const string& filename) {
    ifstream file(filename, ios::ate | ios::binary);
    if (!file.is_open()) {
        throw runtime_error("Failed to open file.");
    }

    size_t fileSize = (size_t)file.tellg();
    vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    return buffer;
}

VkShaderModule VulkanCore::createShaderModule(const vector<char>& code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw runtime_error("Failed to create shader module.");
    }

    return shaderModule;
}

void VulkanCore::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw runtime_error("Failed to create GPU buffer.");
    }

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw runtime_error("Failed to allocate GPU memory.");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

uint32_t VulkanCore::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    throw runtime_error("Failed to find suitable GPU memory type.");
}

void VulkanCore::cleanup() {
    vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
    vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
    vkDestroyFence(device, inFlightFence, nullptr);

    vkDestroyCommandPool(device, commandPool, nullptr);

    vkDestroyImageView(device, computeImageView, nullptr);
    vkDestroyImage(device, computeImage, nullptr);
    vkFreeMemory(device, computeImageMemory, nullptr);

    for (auto imageView : swapChainImageViews) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(device, swapChain, nullptr);

    vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

    vkDestroyBuffer(device, materialBuffer, nullptr);
    vkFreeMemory(device, materialMemory, nullptr);

    vkDestroyBuffer(device, sphereBuffer, nullptr);
    vkFreeMemory(device, sphereMemory, nullptr);

    vkDestroyBuffer(device, triangleBuffer, nullptr);
    vkFreeMemory(device, triangleMemory, nullptr);

    vkDestroyBuffer(device, lightBuffer, nullptr);
    vkFreeMemory(device, lightMemory, nullptr);

    vkDestroyBuffer(device, planeBuffer, nullptr);
    vkFreeMemory(device, planeMemory, nullptr);

    vkDestroyBuffer(device, quadBuffer, nullptr);
    vkFreeMemory(device, quadMemory, nullptr);

    vkDestroyBuffer(device, cubeBuffer, nullptr);
    vkFreeMemory(device, cubeMemory, nullptr);

    vkDestroyBuffer(device, bvhBuffer, nullptr);
    vkFreeMemory(device, bvhMemory, nullptr);

    vkDestroyPipeline(device, computePipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);

    glfwDestroyWindow(window);
    glfwTerminate();
}