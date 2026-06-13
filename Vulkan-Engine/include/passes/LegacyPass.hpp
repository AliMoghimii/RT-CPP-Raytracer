#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include <string>

struct CameraPushConstants;

class LegacyPass {
public:
    struct CreateInfo {
        VkDevice device;
        VkDescriptorPool pool;
        VkDescriptorSetLayout sceneLayout;
        VkBuffer materialBuf, sphereBuf, triangleBuf, lightBuf,
                 planeBuf, quadBuf, cubeBuf, bvhBuf;
        VkBuffer photonBuf, photonCounterBuf, gridHeadBuf, photonNextBuf;
        VkBuffer instanceBuf, tlasBuf;
        VkSampler sampler;
        const std::vector<VkImageView>* texViews;
        VkImageView outputView;   // ldrImage - binding 0, rgba8 write
        VkImageView fallbackView; // hdrImage - fills unused texture slots
    };

    void create(const CreateInfo& info);
    void record(VkCommandBuffer cmd, const CameraPushConstants& pc,
                uint32_t dX, uint32_t dY);
    void destroy(VkDevice device);

    // Frees descSet only -- pipelines/pipelineLayout are left intact. Used when swapping
    // scenes: the buffers/textures behind the descriptor set are about to be destroyed and
    // recreated, but the compute pipelines don't need to change.
    void releaseDescriptorSet(VkDevice device, VkDescriptorPool pool);

    // Re-allocates and re-writes descSet against (possibly new) buffer/texture handles.
    // Call after releaseDescriptorSet() once the new scene's GPU resources exist.
    void rebindDescriptorSet(const CreateInfo& info);

private:
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet descSet = VK_NULL_HANDLE;

    VkPipeline photonPipeline = VK_NULL_HANDLE;
    VkBuffer photonCounterBuf = VK_NULL_HANDLE;
    VkBuffer gridHeadBuf = VK_NULL_HANDLE;

    void allocateAndWriteDescriptorSet(const CreateInfo& info);

    static std::vector<char> readSpv(const std::string& path);
};
