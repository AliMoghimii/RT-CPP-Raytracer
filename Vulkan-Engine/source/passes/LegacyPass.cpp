#include "passes/LegacyPass.hpp"
#include "renderer/Renderer.hpp"

#include <fstream>
#include <stdexcept>
#include <vector>

std::vector<char> LegacyPass::readSpv(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("LegacyPass: failed to open " + path);
    size_t size = (size_t)file.tellg();
    std::vector<char> buf(size);
    file.seekg(0);
    file.read(buf.data(), (std::streamsize)size);
    return buf;
}

void LegacyPass::create(const CreateInfo& info) {
    // --- Pipeline layout: 1 descriptor set + CameraPushConstants ---
    VkPushConstantRange pcRange{};
    pcRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pcRange.offset = 0;
    pcRange.size = sizeof(CameraPushConstants);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &info.sceneLayout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcRange;

    if (vkCreatePipelineLayout(info.device, &plci, nullptr, &pipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("LegacyPass: pipeline layout creation failed.");

    // --- Compute pipeline from raytracer.comp.spv ---
    auto code = readSpv("shaders/legacy/raytracer.comp.spv");

    VkShaderModuleCreateInfo smci{};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = code.size();
    smci.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule mod;
    if (vkCreateShaderModule(info.device, &smci, nullptr, &mod) != VK_SUCCESS)
        throw std::runtime_error("LegacyPass: shader module creation failed.");

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = mod;
    stage.pName = "main";

    VkComputePipelineCreateInfo cpci{};
    cpci.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.layout = pipelineLayout;
    cpci.stage  = stage;

    if (vkCreateComputePipelines(info.device, VK_NULL_HANDLE, 1, &cpci, nullptr, &pipeline) != VK_SUCCESS)
        throw std::runtime_error("LegacyPass: pipeline creation failed.");

    vkDestroyShaderModule(info.device, mod, nullptr);

    // --- Descriptor set (reuses sceneDescSetLayout, binding 0 = ldrImage) ---
    VkDescriptorSetAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = info.pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &info.sceneLayout;

    if (vkAllocateDescriptorSets(info.device, &ai, &descSet) != VK_SUCCESS)
        throw std::runtime_error("LegacyPass: descriptor set allocation failed.");

    // binding 0: output storage image (ldrImage, rgba8)
    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageView = info.outputView;
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    // bindings 1-8: scene SSBOs
    VkBuffer sceneBuffers[8] = {
        info.materialBuf, info.sphereBuf, info.triangleBuf, info.lightBuf,
        info.planeBuf, info.quadBuf, info.cubeBuf, info.bvhBuf
    };
    VkDescriptorBufferInfo bufInfos[8];
    for (int i = 0; i < 8; i++) {
        bufInfos[i].buffer = sceneBuffers[i];
        bufInfos[i].offset = 0;
        bufInfos[i].range = VK_WHOLE_SIZE;
    }

    // binding 9: texture sampler array (100 slots)
    std::vector<VkDescriptorImageInfo> texInfos(100);
    const auto& views = *info.texViews;
    for (int i = 0; i < 100; i++) {
        texInfos[i].sampler = info.sampler;
        if (i < (int)views.size()) {
            texInfos[i].imageView = views[i];
            texInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        } else {
            texInfos[i].imageView = info.fallbackView;
            texInfos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
    }

    VkWriteDescriptorSet writes[10] = {};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[0].pImageInfo = &imgInfo;

    for (int i = 0; i < 8; i++) {
        writes[i + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i + 1].dstSet = descSet;
        writes[i + 1].dstBinding = (uint32_t)(i + 1);
        writes[i + 1].descriptorCount = 1;
        writes[i + 1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[i + 1].pBufferInfo = &bufInfos[i];
    }

    writes[9].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[9].dstSet = descSet;
    writes[9].dstBinding = 9;
    writes[9].descriptorCount = 100;
    writes[9].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[9].pImageInfo = texInfos.data();

    vkUpdateDescriptorSets(info.device, 10, writes, 0, nullptr);
}

void LegacyPass::record(VkCommandBuffer cmd, const CameraPushConstants& pc,
                        uint32_t dX, uint32_t dY) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
        pipelineLayout, 0, 1, &descSet, 0, nullptr);
    vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT,
        0, sizeof(CameraPushConstants), &pc);
    vkCmdDispatch(cmd, dX, dY, 1);
}

void LegacyPass::destroy(VkDevice device) {
    if (pipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, pipeline, nullptr);
    if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    pipeline = VK_NULL_HANDLE;
    pipelineLayout = VK_NULL_HANDLE;
    descSet = VK_NULL_HANDLE;
}
