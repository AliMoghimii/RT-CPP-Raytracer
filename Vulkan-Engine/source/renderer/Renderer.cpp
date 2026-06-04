#include "renderer/Renderer.hpp"
#include "scene/ImageLoader.hpp"
#include "gi/RCPushConstants.hpp"

#include "imgui-docking/imgui.h"

#include <stdexcept>
#include <vector>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstring>

using namespace std;

// ---- Public API ----

void Renderer::run() {
    initWindow();
    initVulkan();
    glfwShowWindow(window);
    mainLoop();
    cleanup();
}

void Renderer::loadScene(
    const vector<GPUMaterial>& mats, const vector<GPUSphere>& sphs,
    const vector<GPUTriangle>& tris, const vector<GPULight>& lghts,
    const vector<GPUPlane>& plns, const vector<GPUQuad>& quds,
    const vector<GPUCube>& cbs, const vector<GPUBVHNode>&  bvh)
{
    sceneMaterials = mats;
    sceneSpheres = sphs;
    sceneTriangles = tris;
    sceneLights = lghts;
    scenePlanes = plns;
    sceneQuads = quds;
    sceneCubes = cbs;
    sceneBVH = bvh;
}

void Renderer::loadTextures(const vector<string>& paths) {
    pendingTexturePaths = paths;
}

// ---- Initialization ----

void Renderer::initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    window = glfwCreateWindow(1280, 720, "Vulkan Real-Time Raytracer", nullptr, nullptr);
}

void Renderer::initVulkan() {
    ctx.initialize(window);
    swapchain.create(ctx, ctx.surface, { 1280, 720 });

    gbuffer.create(ctx, swapchain.extent);
    cmdManager.create(ctx, (uint32_t)swapchain.images.size());

    // SAMPLED_BIT allows hdrImage to serve as fallback in the texture descriptor array
    hdrImage = Image(ctx.allocator, swapchain.extent.width, swapchain.extent.height,
                     VK_FORMAT_R16G16B16A16_SFLOAT,
                     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VMA_MEMORY_USAGE_GPU_ONLY);

    // ldrImage is the tonemap output, blitted to the swapchain each frame:
    ldrImage = Image(ctx.allocator, swapchain.extent.width, swapchain.extent.height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY);

    VkCommandBuffer cmd = cmdManager.beginOneTime(ctx);
    gbuffer.transitionForWrite(cmd);
    transitionImageLayout(cmd, hdrImage.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    transitionImageLayout(cmd, ldrImage.handle, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    cmdManager.submitOneTime(ctx, cmd);

    createTextureSampler();
    createTextureResources();
    createSceneBuffers();
    rcStorage.initialize(ctx, rcConfig);
    prevFrameSlots.assign(rcConfig.numCascades, 0);
    for (int i = 0; i < rcConfig.numCascades; i++)
        prevFrameSlots[i] = rcStorage.maxActiveSlots[i];  // fallback for frame 0
    createSceneDescriptorSetLayout();
    createGBufferDescriptorSetLayout();
    createRCDescriptorSetLayouts();
    createPipelines();
    createRCPipelineLayouts();
    createDescriptorPool();
    createSceneDescriptorSet();
    createGBufferDescriptorSet();
    createRCDescriptorSets();
    legacyPass.create({
        ctx.device, descriptorPool, sceneDescSetLayout,
        materialBuffer.handle, sphereBuffer.handle, triangleBuffer.handle, lightBuffer.handle,
        planeBuffer.handle, quadBuffer.handle, cubeBuffer.handle, bvhBuffer.handle,
        photonBuffer.handle, photonCounterBuffer.handle, gridHeadBuffer.handle, photonNextBuffer.handle,
        textureSampler, &textureImageViews,
        ldrImage.view,  // binding 0: rgba8 output
        hdrImage.view   // fallback for unused texture slots
    });
    createTimestampQueryPool();
    debugUI.init(ctx, swapchain, window);
}

// ---- Scene Buffers ----

void Renderer::createSceneBuffers() {
    auto make = [&](size_t sz) {
        return Buffer(ctx.allocator, sz, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    };

    materialBuffer = make(sizeof(GPUMaterial) * max((size_t)1, sceneMaterials.size()));
    sphereBuffer = make(sizeof(GPUSphere) * max((size_t)1, sceneSpheres.size()));
    triangleBuffer = make(sizeof(GPUTriangle) * max((size_t)1, sceneTriangles.size()));
    lightBuffer = make(sizeof(GPULight) * max((size_t)1, sceneLights.size()));
    planeBuffer = make(sizeof(GPUPlane) * max((size_t)1, scenePlanes.size()));
    quadBuffer = make(sizeof(GPUQuad) * max((size_t)1, sceneQuads.size()));
    cubeBuffer = make(sizeof(GPUCube) * max((size_t)1, sceneCubes.size()));
    bvhBuffer = make(sizeof(GPUBVHNode) * max((size_t)1, sceneBVH.size()));

    // --- Caustics Buffers ---
    uint32_t maxPhotons = 5000000;
    uint32_t gridCells = 256 * 256 * 256;

    photonBuffer = Buffer(ctx.allocator, sizeof(GPUPhoton) * maxPhotons,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    photonCounterBuffer = Buffer(ctx.allocator, sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    gridHeadBuffer = Buffer(ctx.allocator, sizeof(int) * gridCells,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    photonNextBuffer = Buffer(ctx.allocator, sizeof(int) * maxPhotons,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    uploadSceneData();
}

void Renderer::uploadSceneData() {
    if (!sceneMaterials.empty()) memcpy(materialBuffer.mapped, sceneMaterials.data(), sizeof(GPUMaterial) * sceneMaterials.size());
    if (!sceneSpheres.empty()) memcpy(sphereBuffer.mapped, sceneSpheres.data(), sizeof(GPUSphere) * sceneSpheres.size());
    if (!sceneTriangles.empty()) memcpy(triangleBuffer.mapped, sceneTriangles.data(), sizeof(GPUTriangle) * sceneTriangles.size());
    if (!sceneLights.empty()) memcpy(lightBuffer.mapped, sceneLights.data(), sizeof(GPULight) * sceneLights.size());
    if (!scenePlanes.empty()) memcpy(planeBuffer.mapped, scenePlanes.data(), sizeof(GPUPlane) * scenePlanes.size());
    if (!sceneQuads.empty()) memcpy(quadBuffer.mapped, sceneQuads.data(), sizeof(GPUQuad) * sceneQuads.size());
    if (!sceneCubes.empty()) memcpy(cubeBuffer.mapped, sceneCubes.data(), sizeof(GPUCube) * sceneCubes.size());
    if (!sceneBVH.empty()) memcpy(bvhBuffer.mapped, sceneBVH.data(), sizeof(GPUBVHNode) * sceneBVH.size());
}

// ---- Textures ----

void Renderer::createTextureSampler() {
    VkSamplerCreateInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR;
    si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.anisotropyEnable = VK_TRUE;
    si.maxAnisotropy = 16.0f;
    si.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(ctx.device, &si, nullptr, &textureSampler) != VK_SUCCESS)
        throw runtime_error("Renderer: sampler creation failed.");
}

void Renderer::createTextureResources() {
    for (const auto& path : pendingTexturePaths) {
        auto img = ImageLoader::loadPixels(path);
        VkDeviceSize imgSize = (VkDeviceSize)img.width * img.height * 4;

        Buffer staging(ctx.allocator, imgSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
        memcpy(staging.mapped, img.pixels, imgSize);
        ImageLoader::freePixels(img);

        VkImageCreateInfo ici{};
        ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.extent = { (uint32_t)img.width, (uint32_t)img.height, 1 };
        ici.mipLevels = 1;
        ici.arrayLayers = 1;
        ici.format = VK_FORMAT_R8G8B8A8_UNORM;
        ici.tiling = VK_IMAGE_TILING_OPTIMAL;
        ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        ici.samples = VK_SAMPLE_COUNT_1_BIT;

        VkImage texImage;
        VmaAllocation texAlloc;
        VmaAllocationCreateInfo aci{};
        aci.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        if (vmaCreateImage(ctx.allocator, &ici, &aci, &texImage, &texAlloc, nullptr) != VK_SUCCESS)
            throw runtime_error("Renderer: texture image creation failed.");

        VkCommandBuffer cmd = cmdManager.beginOneTime(ctx);
        transitionImageLayout(cmd, texImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = { (uint32_t)img.width, (uint32_t)img.height, 1 };
        vkCmdCopyBufferToImage(cmd, staging.handle, texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        transitionImageLayout(cmd, texImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        cmdManager.submitOneTime(ctx, cmd);

        VkImageViewCreateInfo vci{};
        vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image = texImage;
        vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vci.format = VK_FORMAT_R8G8B8A8_UNORM;
        vci.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

        VkImageView texView;
        if (vkCreateImageView(ctx.device, &vci, nullptr, &texView) != VK_SUCCESS)
            throw runtime_error("Renderer: texture view creation failed.");

        textureImages.push_back(texImage);
        textureImageAllocs.push_back(texAlloc);
        textureImageViews.push_back(texView);
    }
}

// ---- Descriptor Set Layouts ----

void Renderer::createSceneDescriptorSetLayout() {
    vector<VkDescriptorSetLayoutBinding> bindings(14);

    // binding 0: placeholder storage image (the two-pass shaders don't declare it but the layout must match)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // bindings 1-8: scene SSBOs (materials, spheres, triangles, lights, planes, quads, cubes, bvh)
    for (uint32_t i = 1; i <= 8; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    // binding 9: texture array
    bindings[9].binding = 9;
    bindings[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[9].descriptorCount = 100;
    bindings[9].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // bindings 10-13: Caustic Buffers
    for (uint32_t i = 10; i <= 13; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 14;
    ci.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &sceneDescSetLayout) != VK_SUCCESS)
        throw runtime_error("Renderer: scene descriptor set layout creation failed.");
}

void Renderer::createGBufferDescriptorSetLayout() {
    vector<VkDescriptorSetLayoutBinding> bindings(6);
    for (uint32_t i = 0; i < 6; i++) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    }

    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = 6;
    ci.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &gbufDescSetLayout) != VK_SUCCESS)
        throw runtime_error("Renderer: G-buffer descriptor set layout creation failed.");
}

void Renderer::createRCDescriptorSetLayouts() {

    // RC hash set: 7 SSBOs
    // b0=hashKeys, b1=hashValues, b2=slotToKey, b3=slotCounter, b4=cascadeData,
    // b5=parentCascadeData (merge only), b6=shCoeffs (SH pre-integration, probe_sh + gather)
    {
        VkDescriptorSetLayoutBinding binds[7] = {};
        for (uint32_t i = 0; i < 7; i++) {
            binds[i] = { i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
        }
        VkDescriptorSetLayoutCreateInfo ci{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 7, binds };
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &rcHashDescSetLayout) != VK_SUCCESS)
            throw std::runtime_error("Renderer: rcHashDescSetLayout creation failed.");
    }

    // Parent hash set: 4 SSBOs - parent level's keys, values, slotToKey, counter (all read-only)
    // Used in: alloc (mode 1) reads parent slotToKey; merge reads parent keys+values for lookupInParent.
    {
        VkDescriptorSetLayoutBinding binds[4] = {};
        for (uint32_t i = 0; i < 4; i++) {
            binds[i] = { i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
        }
        VkDescriptorSetLayoutCreateInfo ci{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 4, binds };
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &rcParentHashDescSetLayout) != VK_SUCCESS)
            throw std::runtime_error("Renderer: rcParentHashDescSetLayout creation failed.");
    }

    // Tonemap set: 2 storage images - b0 = inHDR (read), b1 = outLDR (write)
    {
        VkDescriptorSetLayoutBinding binds[2] = {};
        for (uint32_t i = 0; i < 2; i++) {
            binds[i] = { i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr };
        }
        VkDescriptorSetLayoutCreateInfo ci{
            VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 2, binds };
        if (vkCreateDescriptorSetLayout(ctx.device, &ci, nullptr, &tonemapDescSetLayout) != VK_SUCCESS)
            throw std::runtime_error("Renderer: tonemapDescSetLayout creation failed.");
    }
}

// ---- Pipelines ----

void Renderer::createPipelines() {
    VkDescriptorSetLayout setLayouts[] = { sceneDescSetLayout, gbufDescSetLayout };

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(CameraPushConstants);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 2;
    plci.pSetLayouts = setLayouts;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(ctx.device, &plci, nullptr, &twoPassPipelineLayout) != VK_SUCCESS)
        throw runtime_error("Renderer: pipeline layout creation failed.");

    primaryPassPipeline = createComputePipelineFromSpv("shaders/visibility/primary.comp.spv", twoPassPipelineLayout);
    photonPipeline = createComputePipelineFromSpv("shaders/photon-mapping/photonpass.comp.spv", twoPassPipelineLayout);
    compositePassPipeline = createComputePipelineFromSpv("shaders/shading/composite_temp.comp.spv", twoPassPipelineLayout);
}

void Renderer::createRCPipelineLayouts() {
    auto makeLayout = [&](std::initializer_list<VkDescriptorSetLayout> setLayouts,
        uint32_t pcSize) -> VkPipelineLayout {
            std::vector<VkDescriptorSetLayout> layouts(setLayouts);
            VkPushConstantRange pcRange{ VK_SHADER_STAGE_COMPUTE_BIT, 0, pcSize };
            VkPipelineLayoutCreateInfo ci{};
            ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            ci.setLayoutCount = (uint32_t)layouts.size();
            ci.pSetLayouts = layouts.data();
            ci.pushConstantRangeCount = 1;
            ci.pPushConstantRanges = &pcRange;
            VkPipelineLayout layout;
            if (vkCreatePipelineLayout(ctx.device, &ci, nullptr, &layout) != VK_SUCCESS)
                throw std::runtime_error("Renderer: RC pipeline layout creation failed.");
            return layout;
        };

    // Alloc: set0=gbufDescSetLayout (G-buffer pixels for cascade-0),
    //        set1=rcHashDescSetLayout (target level's hash map, write),
    //        set2=rcParentHashDescSetLayout (parent level's map, read — for cascade k>0)
    rcAllocPipelineLayout = makeLayout(
        { gbufDescSetLayout, rcHashDescSetLayout, rcParentHashDescSetLayout },
        sizeof(RCAllocPC));

    // Trace: set0=sceneDescSetLayout (BVH + materials + textures),
    //        set1=rcHashDescSetLayout (reads slotToKey, writes cascadeData)
    rcTracePipelineLayout = makeLayout(
        { sceneDescSetLayout, rcHashDescSetLayout },
        sizeof(RCTracePC));

    // Merge: set0=rcHashDescSetLayout (current level, read+write cascadeData),
    //        set1=rcParentHashDescSetLayout (parent level, read-only)
    rcMergePipelineLayout = makeLayout(
        { rcHashDescSetLayout, rcParentHashDescSetLayout },
        sizeof(RCMergePC));

    // SH pre-integration: set0=rcHashDescSetLayout (reads cascadeData b4, writes shCoeffs b6)
    struct RCSHPushConstants { int octRes; int maxActiveSlots; };
    rcSHPipelineLayout = makeLayout({ rcHashDescSetLayout }, sizeof(RCSHPushConstants));

    // Gather: set0=sceneDescSetLayout (BVH + lights + materials, matches bvh.glsl/lighting.glsl bindings),
    //         set1=gbufDescSetLayout  (G-buffer read + hdrImage write at b5),
    //         set2=rcHashDescSetLayout (cascade-0 read)
    rcGatherPipelineLayout = makeLayout(
        { sceneDescSetLayout, gbufDescSetLayout, rcHashDescSetLayout },
        sizeof(RCGatherPC));

    // Transparent: same three descriptor sets as gather; separate push constant (128 bytes, includes camera orientation)
    rcTransparentPipelineLayout = makeLayout(
        { sceneDescSetLayout, gbufDescSetLayout, rcHashDescSetLayout },
        sizeof(TransparentPC));

    // ProbeDebug: set0=gbufDescSetLayout (gPosition b0 + hdrImage b5),
    //             set1=rcHashDescSetLayout (cascade-k hash + cascadeData)
    rcProbeDebugPipelineLayout = makeLayout(
        { gbufDescSetLayout, rcHashDescSetLayout },
        sizeof(ProbeDebugPC));

    // Tonemap: set0=tonemapDescSetLayout (b0=inHDR read, b1=outLDR write)
    tonemapPipelineLayout = makeLayout({ tonemapDescSetLayout }, sizeof(TonemapPC));

    rcAllocPipeline = createComputePipelineFromSpv(
        "shaders/rc/probe_alloc.comp.spv", rcAllocPipelineLayout);
    rcTracePipeline = createComputePipelineFromSpv(
        "shaders/rc/probe_trace.comp.spv", rcTracePipelineLayout);
    rcMergePipeline = createComputePipelineFromSpv(
        "shaders/rc/cascade_merge.comp.spv", rcMergePipelineLayout);
    rcSHPipeline = createComputePipelineFromSpv(
        "shaders/rc/probe_sh.comp.spv", rcSHPipelineLayout);
    rcGatherPipeline = createComputePipelineFromSpv(
        "shaders/shading/final_gather.comp.spv", rcGatherPipelineLayout);
    // Reflection pass: reuses rcGatherPipelineLayout (same 3 desc sets + RCGatherPC push constant).
    rcReflectionPipeline = createComputePipelineFromSpv(
        "shaders/shading/reflection.comp.spv", rcGatherPipelineLayout);
    rcTransparentPipeline = createComputePipelineFromSpv(
        "shaders/shading/transparent.comp.spv", rcTransparentPipelineLayout);
    rcProbeDebugPipeline = createComputePipelineFromSpv(
        "shaders/debug/probe_debug.comp.spv", rcProbeDebugPipelineLayout);
    tonemapPipeline = createComputePipelineFromSpv(
        "shaders/tonemap/tonemap.comp.spv", tonemapPipelineLayout);
}

VkPipeline Renderer::createComputePipelineFromSpv(const string& path, VkPipelineLayout layout) {
    auto code = readFile(path);
    VkShaderModule mod = createShaderModule(code);

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = mod;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.layout = layout;
    pipelineInfo.stage = stageInfo;

    VkPipeline pipeline;
    if (vkCreateComputePipelines(ctx.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
        throw runtime_error("Renderer: failed to create compute pipeline from " + path);

    vkDestroyShaderModule(ctx.device, mod, nullptr);
    return pipeline;
}

// ---- Descriptor Pool ----

void Renderer::createDescriptorPool() {
    // Size the pool for the maximum cascade count the slider allows (8), not the current
    // startup value. recreateCascades() frees and re-allocates 2*N+1 RC descriptor sets;
    // if the pool was sized for N=3 and the user applies N=5, allocation fails.
    constexpr int kMaxCascades = 8;

    vector<VkDescriptorPoolSize> poolSizes(3);
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[0].descriptorCount = 10;                                          // 7 gbuf + 2 tonemap + 1 legacyPass
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = uint32_t(24 + kMaxCascades * 7 + kMaxCascades * 4);  // +8 (+4 caustics SSBOs) for both Scene and LegacyPass
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = 200;                                         // 100 sceneDescSet + 100 legacyPass

    VkDescriptorPoolCreateInfo pi{};
    pi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pi.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;              // needed by recreateCascades()
    pi.poolSizeCount = 3;
    pi.pPoolSizes = poolSizes.data();
    pi.maxSets = uint32_t(3 + kMaxCascades + kMaxCascades + 1);               // +1 for legacyPass descSet

    if (vkCreateDescriptorPool(ctx.device, &pi, nullptr, &descriptorPool) != VK_SUCCESS)
        throw runtime_error("Renderer: descriptor pool creation failed.");
}

// ---- Descriptor Sets ----

void Renderer::createSceneDescriptorSet() {
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = descriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &sceneDescSetLayout;

    if (vkAllocateDescriptorSets(ctx.device, &ai, &sceneDescSet) != VK_SUCCESS)
        throw runtime_error("Renderer: scene descriptor set allocation failed.");

    // binding 0: hdrImage as placeholder (storage image)
    VkDescriptorImageInfo placeholderInfo{};
    placeholderInfo.imageView   = hdrImage.view;
    placeholderInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // bindings 1-8: scene SSBOs
    Buffer* bufs[] = {
        &materialBuffer,
        &sphereBuffer,
        &triangleBuffer,
        &lightBuffer,
        &planeBuffer,
        &quadBuffer,
        &cubeBuffer,
        &bvhBuffer
    };

    vector<VkDescriptorBufferInfo> bufInfos(8);
    for (int i = 0; i < 8; i++) {
        bufInfos[i].buffer = bufs[i]->handle;
        bufInfos[i].offset = 0;
        bufInfos[i].range = VK_WHOLE_SIZE;
    }

    // binding 9: texture sampler array — fill unused slots with hdrImage (in GENERAL layout)
    vector<VkDescriptorImageInfo> texInfos(100);
    for (int i = 0; i < 100; i++) {
        texInfos[i].sampler = textureSampler;
        if (i < (int)textureImageViews.size()) {
            texInfos[i].imageView = textureImageViews[i];
            texInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        } else {
            texInfos[i].imageView = hdrImage.view;
            texInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
    }

    // bindings 10-13: Caustics SSBOs
    Buffer* causticBufs[] = {
        &photonBuffer,
        &photonCounterBuffer,
        &gridHeadBuffer,
        &photonNextBuffer
    };

    vector<VkDescriptorBufferInfo> causticBufInfos(4);
    for (int i = 0; i < 4; i++) {
        causticBufInfos[i].buffer = causticBufs[i]->handle;
        causticBufInfos[i].offset = 0;
        causticBufInfos[i].range = VK_WHOLE_SIZE;
    }

    vector<VkWriteDescriptorSet> writes(14);

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = sceneDescSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].pImageInfo = &placeholderInfo;

    for (int i = 0; i < 8; i++) {
        writes[i + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i + 1].dstSet = sceneDescSet;
        writes[i + 1].dstBinding = (uint32_t)(i + 1);
        writes[i + 1].descriptorCount = 1;
        writes[i + 1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i + 1].pBufferInfo = &bufInfos[i];
    }

    writes[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[9].dstSet = sceneDescSet;
    writes[9].dstBinding = 9;
    writes[9].descriptorCount = 100;
    writes[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[9].pImageInfo = texInfos.data();

    for (int i = 0; i < 4; i++) {
        writes[10 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[10 + i].dstSet = sceneDescSet;
        writes[10 + i].dstBinding = (uint32_t)(10 + i);
        writes[10 + i].descriptorCount = 1;
        writes[10 + i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[10 + i].pBufferInfo = &causticBufInfos[i];
    }

    vkUpdateDescriptorSets(ctx.device, 14, writes.data(), 0, nullptr);
}

void Renderer::createGBufferDescriptorSet() {
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = descriptorPool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &gbufDescSetLayout;

    if (vkAllocateDescriptorSets(ctx.device, &ai, &gbufDescSet) != VK_SUCCESS)
        throw runtime_error("Renderer: G-buffer descriptor set allocation failed.");

    VkImageView views[6] = {
        gbuffer.position.view, gbuffer.normal.view, gbuffer.albedo.view,
        gbuffer.emissive.view, gbuffer.linearDepth.view, hdrImage.view
    };

    vector<VkDescriptorImageInfo> imgInfos(6);
    vector<VkWriteDescriptorSet>  writes(6);

    for (uint32_t i = 0; i < 6; i++) {
        imgInfos[i].imageView = views[i];
        imgInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imgInfos[i].sampler = VK_NULL_HANDLE;

        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = gbufDescSet;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[i].pImageInfo = &imgInfos[i];
    }

    vkUpdateDescriptorSets(ctx.device, 6, writes.data(), 0, nullptr);
}

void Renderer::createRCDescriptorSets() {
    int N = rcConfig.numCascades;

    // --- rcHashDescSets[i]: 6 SSBOs for level i's own hash map ---
    // b0=hashKeys, b1=hashValues, b2=slotToKey, b3=slotCounter, b4=cascadeData
    // b5=parentCascadeData, during alloc/trace, point at the next level's data as a dummy;
    //                       during merge, the merge shader reads it as the parent's radiance.
    rcHashDescSets.resize(N);
    for (int i = 0; i < N; i++) {
        VkDescriptorSetAllocateInfo ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr, descriptorPool, 1, &rcHashDescSetLayout };
        if (vkAllocateDescriptorSets(ctx.device, &ai, &rcHashDescSets[i]) != VK_SUCCESS)
            throw std::runtime_error("Renderer: rcHashDescSets allocation failed.");

        auto& lvl = rcStorage.levels[i];
        VkBuffer parentDataHandle = (i + 1 < N)
            ? rcStorage.levels[i + 1].cascadeData.handle
            : lvl.cascadeData.handle; // last level: self-reference dummy

        VkDescriptorBufferInfo infos[7] = {
            { lvl.hashKeys.handle, 0, VK_WHOLE_SIZE },
            { lvl.hashValues.handle, 0, VK_WHOLE_SIZE },
            { lvl.slotToKey.handle, 0, VK_WHOLE_SIZE },
            { lvl.slotCounter.handle, 0, VK_WHOLE_SIZE },
            { lvl.cascadeData.handle, 0, VK_WHOLE_SIZE },
            { parentDataHandle, 0, VK_WHOLE_SIZE },
            { lvl.shCoeffs.handle, 0, VK_WHOLE_SIZE },
        };
        VkWriteDescriptorSet writes[7] = {};
        for (uint32_t b = 0; b < 7; b++) {
            writes[b] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                rcHashDescSets[i], b, 0, 1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &infos[b], nullptr };
        }
        vkUpdateDescriptorSets(ctx.device, 7, writes, 0, nullptr);
    }

    // --- rcParentHashDescSets[i]: 4 SSBOs pointing at level i's keys/values/slotToKey/counter ---
    // Bound as "set 2" during alloc of cascade k (pass rcParentHashDescSets[k-1]).
    // Bound as "set 1" during merge of cascade k  (pass rcParentHashDescSets[k+1]).
    rcParentHashDescSets.resize(N);
    for (int i = 0; i < N; i++) {
        VkDescriptorSetAllocateInfo ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr, descriptorPool, 1, &rcParentHashDescSetLayout };
        if (vkAllocateDescriptorSets(ctx.device, &ai, &rcParentHashDescSets[i]) != VK_SUCCESS)
            throw std::runtime_error("Renderer: rcParentHashDescSets allocation failed.");

        auto& lvl = rcStorage.levels[i];
        VkDescriptorBufferInfo infos[4] = {
            { lvl.hashKeys.handle, 0, VK_WHOLE_SIZE },
            { lvl.hashValues.handle, 0, VK_WHOLE_SIZE },
            { lvl.slotToKey.handle, 0, VK_WHOLE_SIZE },
            { lvl.slotCounter.handle, 0, VK_WHOLE_SIZE },
        };
        VkWriteDescriptorSet writes[4] = {};
        for (uint32_t b = 0; b < 4; b++) {
            writes[b] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                rcParentHashDescSets[i], b, 0, 1,
                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &infos[b], nullptr };
        }
        vkUpdateDescriptorSets(ctx.device, 4, writes, 0, nullptr);
    }

    // --- tonemapDescSet: b0=inHDR (hdrImage, read), b1=outLDR (ldrImage, write) ---
    {
        VkDescriptorSetAllocateInfo ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            nullptr, descriptorPool, 1, &tonemapDescSetLayout };
        if (vkAllocateDescriptorSets(ctx.device, &ai, &tonemapDescSet) != VK_SUCCESS)
            throw std::runtime_error("Renderer: tonemapDescSet allocation failed.");

        VkDescriptorImageInfo imgInfos[2] = {
            { VK_NULL_HANDLE, hdrImage.view, VK_IMAGE_LAYOUT_GENERAL },
            { VK_NULL_HANDLE, ldrImage.view, VK_IMAGE_LAYOUT_GENERAL },
        };
        VkWriteDescriptorSet writes[2] = {};
        for (uint32_t b = 0; b < 2; b++) {
            writes[b] = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
                tonemapDescSet, b, 0, 1,
                VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imgInfos[b], nullptr, nullptr };
        }
        vkUpdateDescriptorSets(ctx.device, 2, writes, 0, nullptr);
    }
}

// ---- Main Loop ----

void Renderer::mainLoop() {
    lastFrame = (float)glfwGetTime();
    float fpsTimer  = 0.0f;
    int frameCount = 0;
    cout << "Renderer: Rendering started.\n";
    bool rcSlotChecked = false;
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        fpsTimer += deltaTime;
        frameCount++;
        if (fpsTimer >= 1.0f) {
            float ms = fpsTimer / frameCount * 1000.0f;
            cout << "FPS: " << frameCount << " | " << ms << "ms"
                 << " | prim=" << lastFrameStats.gpuPrimaryMs
                 << " alloc=" << lastFrameStats.gpuAllocMs
                 << " trace=" << lastFrameStats.gpuTraceMs
                 << " merge=" << lastFrameStats.gpuMergeMs
                 << " gather=" << lastFrameStats.gpuGatherMs
                 << " trans=" << lastFrameStats.gpuTransparentMs
                 << " tone=" << lastFrameStats.gpuTonemapMs << "\n";
            fpsTimer  = 0.0f;
            frameCount = 0;
        }

        glfwPollEvents();
        processInput();
        updateDynamicData();
        drawFrame();

        if (!rcSlotChecked) {
            rcSlotChecked = true;
            vkDeviceWaitIdle(ctx.device);

            printf("[RC] Cascade slot counts:\n");
            uint32_t prevCount = UINT32_MAX;
            bool hierarchyOK = true;

            for (int lvl = 0; lvl < rcConfig.numCascades; lvl++) {
                Buffer rb(ctx.allocator, sizeof(uint32_t),
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_TO_CPU);
                VkCommandBuffer cmd = cmdManager.beginOneTime(ctx);
                VkBufferCopy region{ 0, 0, sizeof(uint32_t) };
                vkCmdCopyBuffer(cmd, rcStorage.levels[lvl].slotCounter.handle, rb.handle, 1, &region);
                cmdManager.submitOneTime(ctx, cmd);

                uint32_t count = *reinterpret_cast<uint32_t*>(rb.mapped);
                uint32_t maxSlots = rcStorage.maxActiveSlots[lvl];

                printf("  cascade-%d: %5u probes  (max=%u)  %s\n",
                    lvl, count, maxSlots,
                    (count == 0) ? "WARNING: empty!" :
                    (count > maxSlots) ? "ERROR: overflow!" : "OK");

                if (lvl > 0 && count > prevCount) {
                    printf("    ERROR: cascade-%d has MORE probes than cascade-%d\n", lvl, lvl - 1);
                    hierarchyOK = false;
                }
                prevCount = count;
            }

            if (hierarchyOK)
                printf("  Hierarchy invariant OK (each level <= previous)\n");
            printf("  Remove this block once counts look correct.\n");
        }
    }
    vkDeviceWaitIdle(ctx.device);
}

void Renderer::processInput() {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    float cameraSpeed = 3.5f * deltaTime;
    float rotSpeed    = 90.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) cameraPos += cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) cameraPos -= cameraSpeed * cameraUp;
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) yaw += rotSpeed;
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) yaw -= rotSpeed;

    glm::vec3 right     = glm::normalize(glm::cross(cameraUp, cameraFront));
    glm::vec3 flatFront = glm::normalize(glm::vec3(cameraFront.x, 0.0f, cameraFront.z));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) cameraPos += cameraSpeed * flatFront;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) cameraPos -= cameraSpeed * flatFront;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) cameraPos -= right * cameraSpeed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) cameraPos += right * cameraSpeed;

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    cameraFront = glm::normalize(front);
}

void Renderer::updateDynamicData() {
    if (sceneSpheres.size() > 4) {
        glm::vec3 eyeRCenter = sceneSpheres[1].center;
        sceneSpheres[2].center = eyeRCenter + glm::normalize(cameraPos - eyeRCenter) * 0.09f;

        glm::vec3 eyeLCenter = sceneSpheres[3].center;
        sceneSpheres[4].center = eyeLCenter + glm::normalize(cameraPos - eyeLCenter) * 0.09f;

        memcpy(sphereBuffer.mapped, sceneSpheres.data(), sizeof(GPUSphere) * sceneSpheres.size());
    }
}

void Renderer::drawFrame() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    if (width == 0 || height == 0) return;

    vkWaitForFences(ctx.device, 1, &cmdManager.inFlightFence, VK_TRUE, UINT64_MAX);

    // Read actual probe counts from previous frame, GPU is now idle, mapped memory is safe.
    for (int lvl = 0; lvl < rcConfig.numCascades; lvl++) {
        const uint32_t* ptr = reinterpret_cast<const uint32_t*>(rcStorage.levels[lvl].slotCounter.mapped);
        if (ptr && *ptr > 0 && *ptr <= rcStorage.maxActiveSlots[lvl])
            prevFrameSlots[lvl] = *ptr;
        else
            prevFrameSlots[lvl] = rcStorage.maxActiveSlots[lvl];
    }

    // Read back GPU timestamps and VRAM stats from the previous frame (GPU is now idle)
    readbackTimestamps();
    lastFrameStats.frameTimeMs = deltaTime * 1000.0f;

    uint32_t imageIndex;
    VkResult acquireResult = vkAcquireNextImageKHR(ctx.device, swapchain.handle, UINT64_MAX,
                                                   cmdManager.imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
        return;

    vkResetFences(ctx.device, 1, &cmdManager.inFlightFence);

    // Build ImGui draw lists (must happen before recordCommandBuffer where we render them)
    debugUI.draw(*this, lastFrameStats);

    vkResetCommandBuffer(cmdManager.buffer, 0);
    recordCommandBuffer(cmdManager.buffer, imageIndex);

    VkSemaphore waitSems[] = { cmdManager.imageAvailableSemaphore };
    VkSemaphore signalSems[] = { cmdManager.renderFinishedSemaphores[imageIndex] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT };

    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.waitSemaphoreCount = 1;
    si.pWaitSemaphores = waitSems;
    si.pWaitDstStageMask = waitStages;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmdManager.buffer;
    si.signalSemaphoreCount = 1;
    si.pSignalSemaphores = signalSems;

    if (vkQueueSubmit(ctx.computeQueue, 1, &si, cmdManager.inFlightFence) != VK_SUCCESS)
        throw std::runtime_error("Renderer: vkQueueSubmit failed");

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = signalSems;
    pi.swapchainCount = 1;
    pi.pSwapchains = &swapchain.handle;
    pi.pImageIndices = &imageIndex;

    vkQueuePresentKHR(ctx.computeQueue, &pi);
}

void Renderer::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &bi);

    // Reset timestamp queries before any writes this frame.
    // vkCmdResetQueryPool requires no device feature (unlike vkResetQueryPool which needs hostQueryReset).
    if (timestampsSupported)
        vkCmdResetQueryPool(cmd, timestampPool, 0, 16);
    if (timestampsSupported)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, 0);

    int      N = (int)rcStorage.levels.size();  // use allocated count; rcConfig.numCascades may lag a frame after Apply
    uint32_t dX = swapchain.extent.width / 16;
    uint32_t dY = swapchain.extent.height / 16;

    // Build camera push constants — shared by both legacy and RC pipeline paths.
    CameraPushConstants pc{};
    pc.camPos = glm::vec4(cameraPos, 0.0f);
    pc.camForward = glm::vec4(cameraFront, 0.0f);
    pc.camRight = glm::vec4(glm::normalize(glm::cross(cameraUp, cameraFront)), 0.0f);
    pc.camUp = glm::vec4(glm::normalize(glm::cross(cameraFront, glm::vec3(pc.camRight))), 0.0f);
    pc.sphereCount = (int)sceneSpheres.size();
    pc.triangleCount = (int)sceneTriangles.size();
    pc.planeCount = (int)scenePlanes.size();
    pc.quadCount = (int)sceneQuads.size();
    pc.cubeCount = (int)sceneCubes.size();
    pc.lightCount = (int)sceneLights.size();
    pc.bvhCount = (int)sceneBVH.size();
    pc.maxDepth = maxDepth;
    pc.shadowRays = shadowRays;
    pc.primaryRaysPerPixel = primaryRaysPerPixel;
    pc.focalDistance = focalDistance;
    pc.lensRadius = lensRadius;
    pc.fogColor = fogColor;
    pc.enableFog = enableFog;
    pc.skyBottomColor = skyBottomColor;
    pc.enableSkybox = enableSkybox;
    pc.skyTopColor = skyTopColor;
    pc.enableTextures = enableTextures;
    pc.totalEmittedPhotons = totalEmittedPhotons;
    pc.enableCaustics = enableCaustics;
    pc.causticIntensity = causticIntensity;
    pc.padding = 0.0f;
    pc.gridMin = glm::vec4(gridMin, 0.0f);
    pc.gridMax = glm::vec4(gridMax, 0.0f);
    pc.gridRes = gridRes;
    pc.gatherRadius = gatherRadius;

    // === 0. Photon mapping and caustics ===
    // This runs for both Legacy and RC pipelines.
    if (enableCaustics == 1) {
        vkCmdFillBuffer(cmd, photonCounterBuffer.handle, 0, sizeof(uint32_t), 0);
        vkCmdFillBuffer(cmd, gridHeadBuffer.handle, 0, sizeof(int) * 256 * 256 * 256, 0xFFFFFFFF);

        VkMemoryBarrier fillBarrier{};
        fillBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        fillBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        fillBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &fillBarrier, 0, nullptr, 0, nullptr);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, photonPipeline);

        VkDescriptorSet photonSets[] = { sceneDescSet, gbufDescSet };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, twoPassPipelineLayout, 0, 2, photonSets, 0, nullptr);

        vkCmdPushConstants(cmd, twoPassPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(CameraPushConstants), &pc);
        vkCmdDispatch(cmd, (pc.totalEmittedPhotons + 255) / 256, 1, 1);

        VkMemoryBarrier pass1Barrier{};
        pass1Barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        pass1Barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        pass1Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &pass1Barrier, 0, nullptr, 0, nullptr);
    }

    if (useLegacyPipeline) {
        // Legacy monolithic raytracer: writes a fully-shaded rgba8 result directly to ldrImage,
        // so the entire RC pipeline and tonemap are skipped.
        legacyPass.record(cmd, pc, dX, dY);
        emitComputeBarrier(cmd);  // ldrImage writes visible before copy to swapchain
        // Write slots 1-15 so readbackTimestamps() (WAIT_BIT on all 16) never blocks.
        // Total time = toMs(0,1); per-pass slots read ~0ms (all written at the same moment).
        if (timestampsSupported)
            for (int i = 1; i < 16; i++)
                vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, i);
    } else {

    // === 1. Clear RC hash tables (transfer stage) ===
    // Probes are fully re-allocated each frame. Reset hash keys to the empty sentinel
    // (0xFFFFFFFF) and slot counters to 0. hashValues/slotToKey don't need clearing
    // because they're only ever read after a valid atomicCompSwap insert wrote them.
    for (int i = 0; i < N; i++) {
        vkCmdFillBuffer(cmd, rcStorage.levels[i].hashKeys.handle, 0, VK_WHOLE_SIZE, 0xFFFFFFFF);
        vkCmdFillBuffer(cmd, rcStorage.levels[i].slotCounter.handle, 0, sizeof(uint32_t), 0);
    }
    emitTransferToComputeBarrier(cmd);  // fill writes visible before allocation shaders read

    // === 2. Primary visibility — fills G-buffer ===
    // primary.comp: one thread per pixel, outputs world-pos / normal / albedo / emissive.

    VkDescriptorSet primarySets[] = { sceneDescSet, gbufDescSet };
    if (timestampsSupported)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, 2);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, primaryPassPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        twoPassPipelineLayout, 0, 2, primarySets, 0, nullptr);
    vkCmdPushConstants(cmd, twoPassPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(CameraPushConstants), &pc);
    vkCmdDispatch(cmd, dX, dY, 1);
    emitComputeBarrier(cmd);  // G-buffer writes visible to alloc shaders
    if (timestampsSupported)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 3);

    // === 3. Probe allocation (all cascades, bottom-up (0 -> N-1)) ===
    // Cascade 0 (allocMode=0): one thread per G-buffer pixel, inserts probe cell keys
    //                          for each pixel's surface position into the level-0 hash map.
    // Cascades 1-N-1 (allocMode=1): one thread per parent slot, propagates occupied cells
    //                               outward by one level. Each level k must complete before level k+1 reads it.
    if (timestampsSupported)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, 4);
    for (int level = 0; level < N; level++) {
        RCAllocPC allocPC{};
        allocPC.gridSizeHashSize = glm::ivec4(rcConfig.gridSize(level), (int)rcStorage.hashTableSize[level]);
        allocPC.worldOriginSpacing = glm::vec4(rcConfig.worldOrigin, rcConfig.spacing(level));
        allocPC.allocMode = (level == 0) ? 0 : 1;
        allocPC.parentMaxSlots = (level == 0) ? 0 : (int)rcStorage.maxActiveSlots[level - 1];

        // set 2 for cascade-0: rcParentHashDescSets[0] is a layout-compatible dummy
        // (allocMode=0 never reads set 2, but the pipeline layout requires a valid binding)
        VkDescriptorSet allocSets[3] = {
            gbufDescSet,
            rcHashDescSets[level],
            (level == 0) ? rcParentHashDescSets[0] : rcParentHashDescSets[level - 1]
        };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcAllocPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            rcAllocPipelineLayout, 0, 3, allocSets, 0, nullptr);
        vkCmdPushConstants(cmd, rcAllocPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(RCAllocPC), &allocPC);

        if (level == 0) {
            // One thread per G-buffer pixel (16x16 workgroup)
            vkCmdDispatch(cmd, (swapchain.extent.width + 15) / 16, (swapchain.extent.height + 15) / 16, 1);
        }
        else {
            // One thread per parent slot (256-wide 1D workgroup)
            vkCmdDispatch(cmd, (prevFrameSlots[level - 1] + 255) / 256, 1, 1);
        }
        emitComputeBarrier(cmd);  // level k hash map complete before level k+1 reads it
    }
    if (timestampsSupported)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 5);

    // === 4. Probe trace  (all cascades) ===
    // Each active probe fires octRes*octRes rays covering its assigned depth interval
    // [intervalStart, intervalEnd]. Results are packed (radiance, transmittance) per
    // direction and written into cascadeData for the merge step.
    if (timestampsSupported)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, 6);
    for (int level = 0; level < N; level++) {
        RCTracePC tracePC{};
        tracePC.gridSizeOctRes = glm::ivec4(rcConfig.gridSize(level), rcConfig.octRes(level));
        tracePC.worldOriginSpacing = glm::vec4(rcConfig.worldOrigin, rcConfig.spacing(level));
        tracePC.intervalStart = rcConfig.intervalStart(level);
        tracePC.intervalEnd = rcConfig.intervalEnd(level);
        tracePC.bvhCount = (int)sceneBVH.size();
        tracePC.maxActiveSlots = (int)rcStorage.maxActiveSlots[level];
        tracePC.lightCount = (int)sceneLights.size();
        tracePC.planeCount = (int)scenePlanes.size();
        tracePC.quadCount = (int)sceneQuads.size();
        tracePC.evaluateDirect    = 1;  // all cascade levels evaluate direct: each interval stores full outgoing radiance per RC theory
        tracePC.softShadowSamples = (level == 0) ? probeSoftShadowSamples : 1;

        VkDescriptorSet traceSets[] = { sceneDescSet, rcHashDescSets[level] };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcTracePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            rcTracePipelineLayout, 0, 2, traceSets, 0, nullptr);
        vkCmdPushConstants(cmd, rcTracePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(RCTracePC), &tracePC);
        {
            uint32_t oR = (uint32_t)rcConfig.octRes(level);
            uint32_t numRays = prevFrameSlots[level] * oR * oR;
            vkCmdDispatch(cmd, (numRays + 255) / 256, 1, 1);
        }
        emitComputeBarrier(cmd);  // trace writes visible to merge (or gather for level 0)
    }
    if (timestampsSupported)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 7);

    // === 5. Cascade merge (top-down (N-2 -> 0)) ===
    // Composites each probe's near-interval with the trilinearly-interpolated far-interval
    // from the parent level: merged.L = near.L + near.T * far.L; merged.T = near.T * far.T.
    // Top-down order is mandatory: parent must be fully merged before child reads it.
    // Cascade N-1 is skipped (no parent; its traced data is already the final far-field).
    if (timestampsSupported)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, 8);
    for (int level = N - 2; level >= 0; level--) {
        int parent = level + 1;
        RCMergePC mergePC{};
        mergePC.currentGridOctRes = glm::ivec4(rcConfig.gridSize(level), rcConfig.octRes(level));
        mergePC.parentGridOctRes = glm::ivec4(rcConfig.gridSize(parent), rcConfig.octRes(parent));
        mergePC.worldOriginCurrentSpacing = glm::vec4(rcConfig.worldOrigin, rcConfig.spacing(level));
        mergePC.parentSpacing = rcConfig.spacing(parent);
        mergePC.currentHashSize = (int)rcStorage.hashTableSize[level];
        mergePC.parentHashSize = (int)rcStorage.hashTableSize[parent];
        mergePC.currentMaxSlots = (int)rcStorage.maxActiveSlots[level];

        // set 0 = current level (read cascadeData + hash for slot lookup, write cascadeData)
        // set 1 = parent level  (read parentHashKeys/Values for probeLookupInParent)
        VkDescriptorSet mergeSets[] = { rcHashDescSets[level], rcParentHashDescSets[parent] };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcMergePipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            rcMergePipelineLayout, 0, 2, mergeSets, 0, nullptr);
        vkCmdPushConstants(cmd, rcMergePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(RCMergePC), &mergePC);
        {
            uint32_t oR = (uint32_t)rcConfig.octRes(level);
            uint32_t numDirs = prevFrameSlots[level] * oR * oR;
            vkCmdDispatch(cmd, (numDirs + 255) / 256, 1, 1);
        }
        emitComputeBarrier(cmd);  // merged data visible to the next lower level
    }
    if (timestampsSupported)
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 9);

    // === 5.5 SH pre-integration (cascade-0 only) ===
    // Converts the 16-direction cascadeData into 4 SH coefficients (L0+L1) per probe.
    // gather/transparent then use a dot-product lookup instead of a 16-iteration inner loop,
    // reducing register pressure and improving GPU occupancy significantly.
    {
        struct RCSHPushConstants { int octRes; int maxActiveSlots; } shPC{
            rcConfig.octRes(0), (int)rcStorage.maxActiveSlots[0] };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcSHPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            rcSHPipelineLayout, 0, 1, &rcHashDescSets[0], 0, nullptr);
        vkCmdPushConstants(cmd, rcSHPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(shPC), &shPC);
        vkCmdDispatch(cmd, (prevFrameSlots[0] + 255) / 256, 1, 1);
        emitComputeBarrier(cmd);  // shCoeffs writes visible to gather + transparent
    }

    // === 6. Final gather (replaces composite_temp) ===
    // Reads cascade-0's fully merged probe data. For each G-buffer pixel: trilinearly
    // interpolates indirect irradiance from the 8 surrounding cascade-0 probes, then adds
    // direct Lambertian + BVH-shadow lighting. Writes HDR result to hdrImage (b5 of gbufDescSet).
    {
        RCGatherPC gatherPC{};
        gatherPC.worldOriginSpacing = glm::vec4(rcConfig.worldOrigin, rcConfig.spacing(0));
        gatherPC.gridSizeOctRes = glm::ivec4(rcConfig.gridSize(0), rcConfig.octRes(0));
        gatherPC.camPos = glm::vec4(cameraPos, 0.0f);
        gatherPC.hashSize = (int)rcStorage.hashTableSize[0];
        gatherPC.bvhCount = (int)sceneBVH.size();
        gatherPC.planeCount = (int)scenePlanes.size();
        gatherPC.quadCount = (int)sceneQuads.size();
        gatherPC.lightCount = (int)sceneLights.size();
        gatherPC.debugMode = (debugMode == 6) ? 5 : debugMode;  // mode 6 uses indirect-only background
        gatherPC.enableDirect = enableDirect;
        gatherPC.enableIndirect = enableIndirect;
        gatherPC.skyBottomColor = glm::vec4(skyBottomColor, 0.0f);
        gatherPC.skyTopColor = glm::vec4(skyTopColor, 0.0f);
        gatherPC.kIndirectScale = kIndirectScale;
        gatherPC.fogDensity = enableFog ? fogDensity : 0.0f;
        gatherPC.fogBlendWithSky = fogBlendWithSky ? 1 : 0;

        // set 0 = sceneDescSet (BVH + lights + materials — matches bvh.glsl/lighting.glsl set 0 bindings)
        // set 1 = gbufDescSet (G-buffer read at b0-b4, hdrImage write at b5)
        // set 2 = rcHashDescSets[0] (cascade-0 hash + cascadeData read)
        VkDescriptorSet gatherSets[] = { sceneDescSet, gbufDescSet, rcHashDescSets[0] };
        if (timestampsSupported)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, 10);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcGatherPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            rcGatherPipelineLayout, 0, 3, gatherSets, 0, nullptr);
        vkCmdPushConstants(cmd, rcGatherPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(RCGatherPC), &gatherPC);
        vkCmdDispatch(cmd, dX, dY, 1);
        emitComputeBarrier(cmd);  // gather writes visible to reflection pass

        // === 6.5 Reflection pass (fog + first-order reflections for all geometry pixels) ===
        // Reads outHDR (from gather_simple), applies fog to ALL geometry pixels, blends
        // reflection BVH color for reflective pixels (~30%). Reuses rcGatherPipelineLayout
        // (same 3 descriptor sets and RCGatherPC push constant) — no new layout needed.
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcReflectionPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            rcGatherPipelineLayout, 0, 3, gatherSets, 0, nullptr);
        vkCmdPushConstants(cmd, rcGatherPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(RCGatherPC), &gatherPC);
        vkCmdDispatch(cmd, dX, dY, 1);
        emitComputeBarrier(cmd);  // reflection writes visible to transparent pass
        if (timestampsSupported)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 11);
    }

    // === 7. Transparent pass (re-traces rays for transparent pixels, overwrites outHDR) ===
    // Reads G-buffer to identify transparent pixels, reconstructs primary rays from camera
    // orientation, then follows the refraction/reflection chain and shades the final hit.
    // Opaque pixels in outHDR are left untouched.
    {
        TransparentPC transPC{};
        transPC.camPos        = pc.camPos;
        transPC.camForward    = pc.camForward;
        transPC.skyBottomColor = glm::vec4(skyBottomColor, 0.0f);
        transPC.skyTopColor    = glm::vec4(skyTopColor,    0.0f);
        transPC.worldOriginSpacing = glm::vec4(rcConfig.worldOrigin, rcConfig.spacing(0));
        transPC.gridSizeOctRes = glm::ivec4(rcConfig.gridSize(0), rcConfig.octRes(0));
        transPC.bvhCount = (int)sceneBVH.size();
        transPC.lightCount = (int)sceneLights.size();
        transPC.planeCount = (int)scenePlanes.size();
        transPC.quadCount = (int)sceneQuads.size();
        transPC.fogBlendWithSky = fogBlendWithSky ? 1 : 0;
        transPC.hashSize = (int)rcStorage.hashTableSize[0];
        transPC.kIndirectScale = kIndirectScale;
        transPC.fogDensity = enableFog ? fogDensity : 0.0f;

        VkDescriptorSet transSets[] = { sceneDescSet, gbufDescSet, rcHashDescSets[0] };
        if (timestampsSupported)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, 12);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcTransparentPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            rcTransparentPipelineLayout, 0, 3, transSets, 0, nullptr);
        vkCmdPushConstants(cmd, rcTransparentPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(TransparentPC), &transPC);
        vkCmdDispatch(cmd, dX, dY, 1);
        emitComputeBarrier(cmd);  // hdrImage writes visible to tonemap
        if (timestampsSupported)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 13);
    }

    // === 7.5. Probe debug overlay (mode 6 only) ===
    // Fires rays from camera and tests against active cascade-k probe octahedra + ray stubs.
    // Each octahedron face is colored by the actual stored directional radiance. Capsule ray
    // stubs are colored per-direction. Writes over hdrImage only at hit pixels.
    if (debugMode == 6) {
        int lvl = probeVizLevel;
        ProbeDebugPC dbgPC{};
        dbgPC.worldOriginSpacing = glm::vec4(rcConfig.worldOrigin, rcConfig.spacing(lvl));
        dbgPC.gridSizeOctRes     = glm::ivec4(rcConfig.gridSize(lvl), rcConfig.octRes(lvl));
        dbgPC.camPos             = pc.camPos;
        dbgPC.camRight           = pc.camRight;
        dbgPC.camUp              = pc.camUp;
        dbgPC.camForward         = pc.camForward;
        dbgPC.hashSize           = (int)rcStorage.hashTableSize[lvl];
        dbgPC.probeRadius        = probeVizRadius;
        dbgPC.rayLength          = probeVizRayLen;
        dbgPC.rayRadius          = probeVizRayRad;
        VkDescriptorSet dbgSets[] = { gbufDescSet, rcHashDescSets[lvl] };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rcProbeDebugPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            rcProbeDebugPipelineLayout, 0, 2, dbgSets, 0, nullptr);
        vkCmdPushConstants(cmd, rcProbeDebugPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(ProbeDebugPC), &dbgPC);
        vkCmdDispatch(cmd, dX, dY, 1);
        emitComputeBarrier(cmd);
    }

    // === 8. Tonemap (hdrImage (SFLOAT) -> ldrImage (UNORM)) ===
    // ACES film curve + gamma correction. Reads hdrImage (b0 of tonemapDescSet),
    // writes ldrImage (b1). Both images must be in GENERAL layout (set at init).
    {
        TonemapPC tonemapPC{ exposure, tonemapMode, {0, 0} };
        if (timestampsSupported)
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, timestampPool, 14);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
            tonemapPipelineLayout, 0, 1, &tonemapDescSet, 0, nullptr);
        vkCmdPushConstants(cmd, tonemapPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
            0, sizeof(TonemapPC), &tonemapPC);
        vkCmdDispatch(cmd, dX, dY, 1);
        if (timestampsSupported) {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 15);
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampPool, 1);
        }
        // No barrier needed before the copy — image layout transitions act as execution barriers.
    }

    } // end else (RC pipeline)

    // === 8. Copy ldrImage -> swapchain ===
    // ldrImage is R8G8B8A8_UNORM; the swapchain surface is also R8G8B8A8_UNORM, so
    // vkCmdCopyImage works directly (no format conversion, no vkCmdBlitImage needed).
    transitionImageLayout(cmd, ldrImage.handle,
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transitionImageLayout(cmd, swapchain.images[imageIndex],
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
    copyRegion.extent = { swapchain.extent.width, swapchain.extent.height, 1 };
    vkCmdCopyImage(cmd,
        ldrImage.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapchain.images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion);

    transitionImageLayout(cmd, ldrImage.handle,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    // ImGui overlay: transition swapchain to COLOR_ATTACHMENT, render, then transition to PRESENT
    transitionImageLayout(cmd, swapchain.images[imageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    debugUI.renderOnSwapchain(cmd, swapchain.imageViews[imageIndex], swapchain.extent);
    transitionImageLayout(cmd, swapchain.images[imageIndex],
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(cmd);
}

// ---- Cleanup ----

void Renderer::cleanup() {
    debugUI.shutdown(ctx);
    destroyTimestampQueryPool();

    // Destroy VMA-backed buffers before the allocator goes down
    materialBuffer = Buffer();
    sphereBuffer = Buffer();
    triangleBuffer = Buffer();
    lightBuffer = Buffer();
    planeBuffer = Buffer();
    quadBuffer = Buffer();
    cubeBuffer = Buffer();
    bvhBuffer = Buffer();

    // Destroy Caustics VMA buffers
    photonBuffer = Buffer();
    photonCounterBuffer = Buffer();
    gridHeadBuffer = Buffer();
    photonNextBuffer = Buffer();

    // RC cascade storage (must come before ctx.shutdown destroys the VMA allocator)
    rcStorage.destroy();

    vkDestroySampler(ctx.device, textureSampler, nullptr);
    for (size_t i = 0; i < textureImages.size(); i++) {
        vkDestroyImageView(ctx.device, textureImageViews[i], nullptr);
        vmaDestroyImage(ctx.allocator, textureImages[i], textureImageAllocs[i]);
    }

    gbuffer.destroy(ctx);
    hdrImage.destroy(ctx.allocator);
    ldrImage.destroy(ctx.allocator);

    legacyPass.destroy(ctx.device);

    vkDestroyPipeline(ctx.device, rcAllocPipeline, nullptr);
    vkDestroyPipeline(ctx.device, rcTracePipeline, nullptr);
    vkDestroyPipeline(ctx.device, rcMergePipeline, nullptr);
    vkDestroyPipeline(ctx.device, rcSHPipeline, nullptr);
    vkDestroyPipeline(ctx.device, rcGatherPipeline, nullptr);
    vkDestroyPipeline(ctx.device, rcReflectionPipeline, nullptr);
    vkDestroyPipeline(ctx.device, rcTransparentPipeline, nullptr);
    vkDestroyPipeline(ctx.device, rcProbeDebugPipeline, nullptr);
    vkDestroyPipeline(ctx.device, tonemapPipeline, nullptr);
    vkDestroyPipeline(ctx.device, primaryPassPipeline, nullptr);
    vkDestroyPipeline(ctx.device, compositePassPipeline, nullptr);
    vkDestroyPipeline(ctx.device, photonPipeline, nullptr);
    vkDestroyPipelineLayout(ctx.device, rcAllocPipelineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, rcTracePipelineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, rcMergePipelineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, rcSHPipelineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, rcGatherPipelineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, rcTransparentPipelineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, rcProbeDebugPipelineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, tonemapPipelineLayout, nullptr);
    vkDestroyPipelineLayout(ctx.device, twoPassPipelineLayout, nullptr);

    vkDestroyDescriptorPool(ctx.device, descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, sceneDescSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, gbufDescSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, rcHashDescSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, rcParentHashDescSetLayout, nullptr);
    vkDestroyDescriptorSetLayout(ctx.device, tonemapDescSetLayout, nullptr);

    cmdManager.destroy(ctx);
    swapchain.destroy(ctx);
    ctx.shutdown();

    glfwDestroyWindow(window);
    glfwTerminate();
}

// ---- Utilities ----

void Renderer::transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                     VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange= { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    else {
        throw std::invalid_argument("Renderer: unsupported layout transition.");
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

vector<char> Renderer::readFile(const string& filename) {
    ifstream file(filename, ios::ate | ios::binary);
    if (!file.is_open())
        throw runtime_error("Renderer: failed to open file: " + filename);

    size_t fileSize = (size_t)file.tellg();
    vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    return buffer;
}

void Renderer::emitComputeBarrier(VkCommandBuffer cmd) {
    VkMemoryBarrier b { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &b, 0, nullptr, 0, nullptr);
}

void Renderer::emitTransferToComputeBarrier(VkCommandBuffer cmd) {
    VkMemoryBarrier b { VK_STRUCTURE_TYPE_MEMORY_BARRIER };
    b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 1, &b, 0, nullptr, 0, nullptr);
}

VkShaderModule Renderer::createShaderModule(const vector<char>& code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule mod;
    if (vkCreateShaderModule(ctx.device, &ci, nullptr, &mod) != VK_SUCCESS)
        throw runtime_error("Renderer: shader module creation failed.");
    return mod;
}

// ---- Timestamp Query Pool ----

void Renderer::createTimestampQueryPool() {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(ctx.physicalDevice, &props);
    timestampPeriod = props.limits.timestampPeriod;
    if (timestampPeriod == 0.0f) return;

    uint32_t qfCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &qfCount, nullptr);
    vector<VkQueueFamilyProperties> qfProps(qfCount);
    vkGetPhysicalDeviceQueueFamilyProperties(ctx.physicalDevice, &qfCount, qfProps.data());

    if (ctx.computeQueueFamily >= qfCount) return;
    if (qfProps[ctx.computeQueueFamily].timestampValidBits == 0) return;

    VkQueryPoolCreateInfo qi{};
    qi.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    qi.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qi.queryCount = 16; // 8 pairs: outer(0/1) primary(2/3) alloc(4/5) trace(6/7) merge(8/9) gather(10/11) transparent(12/13) tonemap(14/15)

    if (vkCreateQueryPool(ctx.device, &qi, nullptr, &timestampPool) == VK_SUCCESS)
        timestampsSupported = true;
}

void Renderer::readbackTimestamps() {
    if (!timestampsSupported) return;

    // Skip the first call: the query pool was just created and vkCmdResetQueryPool
    // hasn't run yet (it runs inside recordCommandBuffer which hasn't executed for frame 0).
    static bool firstCall = true;
    if (firstCall) { firstCall = false; return; }

    uint64_t results[16]{};
    VkResult res = vkGetQueryPoolResults(
        ctx.device, timestampPool, 0, 16,
        sizeof(results), results, sizeof(uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

    if (res == VK_SUCCESS) {
        auto toMs = [&](int begin, int end) -> float {
            return float(results[end] - results[begin]) * timestampPeriod * 1e-6f;
        };
        lastFrameStats.gpuTotalMs       = toMs(0,  1);
        lastFrameStats.gpuPrimaryMs     = toMs(2,  3);
        lastFrameStats.gpuAllocMs       = toMs(4,  5);
        lastFrameStats.gpuTraceMs       = toMs(6,  7);
        lastFrameStats.gpuMergeMs       = toMs(8,  9);
        lastFrameStats.gpuGatherMs      = toMs(10, 11);
        lastFrameStats.gpuTransparentMs = toMs(12, 13);
        lastFrameStats.gpuTonemapMs     = toMs(14, 15);
    }

    // VRAM stats via VMA heap budgets
    VmaBudget budgets[VK_MAX_MEMORY_HEAPS]{};
    vmaGetHeapBudgets(ctx.allocator, budgets);

    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(ctx.physicalDevice, &memProps);

    uint64_t usedBytes = 0, budgetBytes = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            usedBytes   += budgets[i].usage;
            budgetBytes += budgets[i].budget;
        }
    }
    lastFrameStats.vramUsedBytes   = usedBytes;
    lastFrameStats.vramBudgetBytes = budgetBytes;
}

void Renderer::destroyTimestampQueryPool() {
    if (timestampPool != VK_NULL_HANDLE) {
        vkDestroyQueryPool(ctx.device, timestampPool, nullptr);
        timestampPool = VK_NULL_HANDLE;
    }
}

// ---- Cascade Recreation ----

void Renderer::recreateCascades() {
    vkDeviceWaitIdle(ctx.device);

    if (!rcHashDescSets.empty())
        vkFreeDescriptorSets(ctx.device, descriptorPool,
            (uint32_t)rcHashDescSets.size(), rcHashDescSets.data());
    if (!rcParentHashDescSets.empty())
        vkFreeDescriptorSets(ctx.device, descriptorPool,
            (uint32_t)rcParentHashDescSets.size(), rcParentHashDescSets.data());
    if (tonemapDescSet != VK_NULL_HANDLE)
        vkFreeDescriptorSets(ctx.device, descriptorPool, 1, &tonemapDescSet);
    rcHashDescSets.clear();
    rcParentHashDescSets.clear();
    tonemapDescSet = VK_NULL_HANDLE;

    rcStorage.destroy();
    rcStorage.initialize(ctx, rcConfig);
    prevFrameSlots.assign(rcConfig.numCascades, 0);
    for (int i = 0; i < rcConfig.numCascades; i++)
        prevFrameSlots[i] = rcStorage.maxActiveSlots[i];
    createRCDescriptorSets();
    probeVizLevel = std::min(probeVizLevel, rcConfig.numCascades - 1);
}
