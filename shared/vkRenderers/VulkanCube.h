#pragma once

#include "shared/vkRenderers/VulkanRendererBase.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
using glm::mat4;

class CubeRenderer : public RendererBase
{
public:
	CubeRenderer(VulkanRenderDevice& vkDev, VulkanImage inDepthTexture, const char* textureFile);
	virtual ~CubeRenderer();

	virtual void fillCommandBuffer(VkCommandBuffer commandBuffer, size_t currentImage) override;

	void updateUniformBuffer(VulkanRenderDevice& vkDev, uint32_t currentImage, const mat4& m);

private:
	VkSampler mTextureSampler;
	VulkanImage mTexture;

	bool createDescriptorSet(VulkanRenderDevice& vkDev);
};
