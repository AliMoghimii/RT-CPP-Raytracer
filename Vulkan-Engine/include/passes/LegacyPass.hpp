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

private:
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSet descSet = VK_NULL_HANDLE;

    VkPipeline photonPipeline = VK_NULL_HANDLE;
    VkBuffer photonCounterBuf = VK_NULL_HANDLE;
    VkBuffer gridHeadBuf = VK_NULL_HANDLE;

    static std::vector<char> readSpv(const std::string& path);
};
