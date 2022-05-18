#pragma once

#include "shared/vkRenderers/VulkanRendererBase.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
using glm::mat4;

#include "shared/scene/VtxData.h"

class MultiMeshRenderer : public RendererBase
{
public:
	MultiMeshRenderer(VulkanRenderDevice& vkDev,
	                  const char* meshFile,
	                  const char* drawDataFile,
	                  const char* materialFile,
	                  const char* vtxShaderFile,
	                  const char* fragShaderFile);

	void fillCommandBuffer(VkCommandBuffer commandBuffer, size_t currentImage) override;


	void updateIndirectBuffers(VulkanRenderDevice& vkDev, size_t currentImage, bool* visibility = nullptr);

	void updateGeometryBuffers(VulkanRenderDevice& vkDev,
	                           uint32_t vertexCount,
	                           uint32_t indexCount,
	                           const void* vertices,
	                           const void* indices);

	void updateMaterialBuffer(VulkanRenderDevice& vkDev, uint32_t materialSize, const void* materialData);

	void updateUniformBuffer(VulkanRenderDevice& vkDev, size_t currentImage, const mat4& m);

	void updateDrawDataBuffer(VulkanRenderDevice& vkDev,
	                          size_t currentImage,
	                          uint32_t drawDataSize,
	                          const void* drawData);

	void updateCountBuffer(VulkanRenderDevice& vkDev, size_t currentImage, uint32_t itemCount);

	virtual ~MultiMeshRenderer();

	uint32_t vertexBufferSize_;
	uint32_t indexBufferSize_;

private:
	// we need a VulkanRenderDevice (that is used all over the code)
	VulkanRenderDevice& vkDev;

	// The following "sizes" are used multiple times after allocation, so they are cached here:
	uint32_t maxVertexBufferSize_;
	uint32_t maxIndexBufferSize_;

	uint32_t maxShapes_;

	uint32_t maxDrawDataSize_;
	uint32_t maxMaterialSize_;

	// We store all the index and vertex data (loaded from MeshData) in a single, large GPU buffer:
	VkBuffer storageBuffer_;
	VkDeviceMemory storageBufferMemory_;

	// This renderer doesn't used any material data (for now), but we declare an empty GPU buffer for it to be used later
	VkBuffer materialBuffer_;
	VkDeviceMemory materialBufferMemory_;

	// For each of the swap chain images, we declare a copy of indirect rendering data
	// TODO: why do we need to allocate these per-swap-chain-image? Probably b/c they change perframe? 
	std::vector<VkBuffer> indirectBuffers_;
	std::vector<VkDeviceMemory> indirectBuffersMemory_;
	std::vector<VkBuffer> drawDataBuffers_;
	std::vector<VkDeviceMemory> drawDataBuffersMemory_;

	// Buffer for draw count
	std::vector<VkBuffer> countBuffers_;
	std::vector<VkDeviceMemory> countBuffersMemory_;

	/* DrawData loaded from file. Converted to indirectBuffers[] and uploaded to drawDataBuffers[] */

	// this vector stores each mesh's information
	std::vector<DrawData> shapes;

	// the container that contains ALL of the loaded data (multiple meshes together)
	MeshData meshData_;

	bool createDescriptorSet(VulkanRenderDevice& vkDev);

	void loadDrawData(const char* drawDataFile);
};
