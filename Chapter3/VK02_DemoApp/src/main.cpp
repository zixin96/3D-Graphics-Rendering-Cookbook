#define VK_NO_PROTOTYPES
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>

#include "shared//Utils.h"
#include "shared/UtilsVulkan.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
using glm::mat4;
using glm::vec3;
using glm::vec4;

const uint32_t SCREEN_WIDTH = 1280;
const uint32_t SCREEN_HEIGHT = 720;

GLFWwindow* window;

struct UniformBuffer
{
	mat4 mvp;
};

static constexpr VkClearColorValue clearValueColor = { 1.0f, 1.0f, 1.0f, 1.0f };

size_t vertexBufferSize;
size_t indexBufferSize;

VulkanInstance vulkanInstance;
VulkanRenderDevice vulkanRenderDevice;

struct VulkanState
{
	// 1. Descriptor set (layout + pool + sets) -> uses uniform buffers, textures, framebuffers
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorPool descriptorPool;
	std::vector<VkDescriptorSet> descriptorSets;

	// 2. 
	std::vector<VkFramebuffer> swapchainFramebuffers;

	// 3. Pipeline & render pass (using DescriptorSets & pipeline state options)
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;

	// 4. Uniform buffer
	std::vector<VkBuffer> uniformBuffers;
	std::vector<VkDeviceMemory> uniformBuffersMemory;

	// 5. Storage Buffer with index and vertex data
	VkBuffer storageBuffer;
	VkDeviceMemory storageBufferMemory;

	// 6. Depth buffer
	VulkanImage depthTexture;

	VkSampler textureSampler;
	VulkanImage texture;
} vkState;

bool createDescriptorSet()
{
	const std::array<VkDescriptorSetLayoutBinding, 4> bindings = {
		descriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
		descriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
		descriptorSetLayoutBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT),
		descriptorSetLayoutBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
	};

	const VkDescriptorSetLayoutCreateInfo layoutInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.bindingCount = static_cast<uint32_t>(bindings.size()),
		.pBindings = bindings.data()
	};

	VK_CHECK(
		vkCreateDescriptorSetLayout(vulkanRenderDevice.device, &layoutInfo, nullptr, &vkState.descriptorSetLayout));

	// allocate one descriptor set to each swap chain image
	std::vector<VkDescriptorSetLayout> layouts(vulkanRenderDevice.swapchainImages.size(), vkState.descriptorSetLayout);

	const VkDescriptorSetAllocateInfo allocInfo = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = vkState.descriptorPool,
		.descriptorSetCount = static_cast<uint32_t>(vulkanRenderDevice.swapchainImages.size()),
		.pSetLayouts = layouts.data()
	};

	vkState.descriptorSets.resize(vulkanRenderDevice.swapchainImages.size());

	VK_CHECK(vkAllocateDescriptorSets(vulkanRenderDevice.device, &allocInfo, vkState.descriptorSets.data()));

	// update descriptor sets with concrete buffer and texture handles (like texture/buffer binding in OpenGL)
	for (size_t i = 0; i < vulkanRenderDevice.swapchainImages.size(); i++)
	{
		const VkDescriptorBufferInfo bufferInfo = {
			.buffer = vkState.uniformBuffers[i],
			.offset = 0,
			.range = sizeof(UniformBuffer)
		};
		const VkDescriptorBufferInfo bufferInfo2 = {
			.buffer = vkState.storageBuffer,
			.offset = 0,
			.range = vertexBufferSize
		};
		const VkDescriptorBufferInfo bufferInfo3 = {
			.buffer = vkState.storageBuffer,
			.offset = vertexBufferSize,
			.range = indexBufferSize
		};
		const VkDescriptorImageInfo imageInfo = {
			.sampler = vkState.textureSampler,
			.imageView = vkState.texture.imageView,
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		};

		const std::array<VkWriteDescriptorSet, 4> descriptorWrites = {
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = vkState.descriptorSets[i],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pBufferInfo = &bufferInfo
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = vkState.descriptorSets[i],
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo2
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = vkState.descriptorSets[i],
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &bufferInfo3
			},
			VkWriteDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = vkState.descriptorSets[i],
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &imageInfo
			},
		};

		vkUpdateDescriptorSets(vulkanRenderDevice.device,
			static_cast<uint32_t>(descriptorWrites.size()),
			descriptorWrites.data(),
			0,
			nullptr);
	}

	return true;
}

bool fillCommandBuffers(size_t i)
{
	// fill in a structure describing a command buffer
	const VkCommandBufferBeginInfo bi =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
		.pInheritanceInfo = nullptr
	};

	const std::array<VkClearValue, 2> clearValues =
	{
		VkClearValue{.color = clearValueColor},
		VkClearValue{.depthStencil = {1.0f, 0}}
	};

	const VkRect2D screenRect = {
		.offset = {0, 0},
		.extent = {.width = SCREEN_WIDTH, .height = SCREEN_HEIGHT}
	};

	VK_CHECK(vkBeginCommandBuffer(vulkanRenderDevice.commandBuffers[i], &bi));

	const VkRenderPassBeginInfo renderPassInfo = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.pNext = nullptr,
		.renderPass = vkState.renderPass,
		.framebuffer = vkState.swapchainFramebuffers[i],
		.renderArea = screenRect,
		.clearValueCount = static_cast<uint32_t>(clearValues.size()),
		.pClearValues = clearValues.data()
	};

	vkCmdBeginRenderPass(vulkanRenderDevice.commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(vulkanRenderDevice.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vkState.graphicsPipeline);

	vkCmdBindDescriptorSets(vulkanRenderDevice.commandBuffers[i],
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		vkState.pipelineLayout,
		0,
		1,
		&vkState.descriptorSets[i],
		0,
		nullptr);

	vkCmdDraw(vulkanRenderDevice.commandBuffers[i], static_cast<uint32_t>(indexBufferSize / (sizeof(unsigned int))), 1,
		0, 0);

	vkCmdEndRenderPass(vulkanRenderDevice.commandBuffers[i]);

	VK_CHECK(vkEndCommandBuffer(vulkanRenderDevice.commandBuffers[i]));

	return true;
}

void updateUniformBuffer(uint32_t currentImage, const void* uboData, size_t uboSize)
{
	void* data = nullptr;
	vkMapMemory(vulkanRenderDevice.device, vkState.uniformBuffersMemory[currentImage], 0, uboSize, 0, &data);
	memcpy(data, uboData, uboSize);
	vkUnmapMemory(vulkanRenderDevice.device, vkState.uniformBuffersMemory[currentImage]);
}

bool createUniformBuffers()
{
	VkDeviceSize bufferSize = sizeof(UniformBuffer);

	vkState.uniformBuffers.resize(vulkanRenderDevice.swapchainImages.size());
	vkState.uniformBuffersMemory.resize(vulkanRenderDevice.swapchainImages.size());

	for (size_t i = 0; i < vulkanRenderDevice.swapchainImages.size(); i++)
	{
		if (!createBuffer(vulkanRenderDevice.device,
			vulkanRenderDevice.physicalDevice,
			bufferSize,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			vkState.uniformBuffers[i],
			vkState.uniformBuffersMemory[i]))
		{
			printf("Fail: buffers\n");
			return false;
		}
	}

	return true;
}

bool initVulkan()
{
	createInstance(&vulkanInstance.instance);

	if (!setupDebugCallbacks(vulkanInstance.instance, &vulkanInstance.messenger, &vulkanInstance.reportCallback))
		exit(EXIT_FAILURE);

	if (glfwCreateWindowSurface(vulkanInstance.instance, window, nullptr, &vulkanInstance.surface))
		exit(EXIT_FAILURE);


	if (!initVulkanRenderDevice(vulkanInstance,
		vulkanRenderDevice,
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		isDeviceSuitable,
		{ .geometryShader = VK_TRUE }))
	{
		exit(EXIT_FAILURE);
	}


	if (!createTexturedVertexBuffer(vulkanRenderDevice,
		"data/rubber_duck/scene.gltf",
		&vkState.storageBuffer,
		&vkState.storageBufferMemory,
		&vertexBufferSize,
		&indexBufferSize) ||
		!createUniformBuffers())
	{
		printf("Cannot create data buffers\n");
		fflush(stdout);
		exit(1);
	}

	createTextureImage(vulkanRenderDevice,
		"data/rubber_duck/textures/Duck_baseColor.png",
		vkState.texture.image,
		vkState.texture.imageMemory);

	createImageView(vulkanRenderDevice.device,
		vkState.texture.image,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_ASPECT_COLOR_BIT,
		&vkState.texture.imageView);

	createTextureSampler(vulkanRenderDevice.device, &vkState.textureSampler);

	createDepthResources(vulkanRenderDevice, SCREEN_WIDTH, SCREEN_HEIGHT, vkState.depthTexture);

	if (!createDescriptorPool(vulkanRenderDevice, 1, 2, 1, &vkState.descriptorPool) ||
		!createDescriptorSet() ||
		!createColorAndDepthRenderPass(vulkanRenderDevice,
			true,
			&vkState.renderPass,
			RenderPassCreateInfo{
				.clearColor_ = true, .clearDepth_ = true,
				.flags_ = eRenderPassBit_First | eRenderPassBit_Last
			}) ||
		!createPipelineLayout(vulkanRenderDevice.device, vkState.descriptorSetLayout, &vkState.pipelineLayout) ||
		!createGraphicsPipeline(vulkanRenderDevice,
			vkState.renderPass,
			vkState.pipelineLayout,
			{
				"data/shaders/chapter03/VK02.vert",
				"data/shaders/chapter03/VK02.frag",
				"data/shaders/chapter03/VK02.geom"
			},
			&vkState.graphicsPipeline))
	{
		printf("Failed to create pipeline\n");
		fflush(stdout);
		exit(0);
	}

	createColorAndDepthFramebuffers(vulkanRenderDevice,
		vkState.renderPass,
		vkState.depthTexture.imageView,
		vkState.swapchainFramebuffers);

	return VK_SUCCESS;
}

void terminateVulkan()
{
	vkDestroyBuffer(vulkanRenderDevice.device, vkState.storageBuffer, nullptr);
	vkFreeMemory(vulkanRenderDevice.device, vkState.storageBufferMemory, nullptr);

	for (size_t i = 0; i < vulkanRenderDevice.swapchainImages.size(); i++)
	{
		vkDestroyBuffer(vulkanRenderDevice.device, vkState.uniformBuffers[i], nullptr);
		vkFreeMemory(vulkanRenderDevice.device, vkState.uniformBuffersMemory[i], nullptr);
	}

	vkDestroyDescriptorSetLayout(vulkanRenderDevice.device, vkState.descriptorSetLayout, nullptr);
	vkDestroyDescriptorPool(vulkanRenderDevice.device, vkState.descriptorPool, nullptr);

	for (auto framebuffer : vkState.swapchainFramebuffers)
	{
		vkDestroyFramebuffer(vulkanRenderDevice.device, framebuffer, nullptr);
	}

	vkDestroySampler(vulkanRenderDevice.device, vkState.textureSampler, nullptr);
	destroyVulkanImage(vulkanRenderDevice.device, vkState.texture);

	destroyVulkanImage(vulkanRenderDevice.device, vkState.depthTexture);

	vkDestroyRenderPass(vulkanRenderDevice.device, vkState.renderPass, nullptr);

	vkDestroyPipelineLayout(vulkanRenderDevice.device, vkState.pipelineLayout, nullptr);
	vkDestroyPipeline(vulkanRenderDevice.device, vkState.graphicsPipeline, nullptr);

	destroyVulkanRenderDevice(vulkanRenderDevice);

	destroyVulkanInstance(vulkanInstance);
}

/* Common main() and drawOverlay() routines for VK04, VK05 and VK06 samples */

bool drawOverlay()
{
	uint32_t imageIndex = 0;
	if (vkAcquireNextImageKHR(vulkanRenderDevice.device,
		vulkanRenderDevice.swapchain,
		0,
		vulkanRenderDevice.imageAvailableSemaphore,
		VK_NULL_HANDLE,
		&imageIndex) != VK_SUCCESS)
	{
		return false;
	}


	VK_CHECK(vkResetCommandPool(vulkanRenderDevice.device, vulkanRenderDevice.commandPool, 0));

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	const float ratio = width / (float)height;

	const mat4 m1 = glm::rotate(
		glm::translate(mat4(1.0f), vec3(0.f, 0.5f, -1.5f)) * glm::rotate(mat4(1.f), glm::pi<float>(),
			vec3(1, 0, 0)),
		(float)glfwGetTime(),
		vec3(0.0f, 1.0f, 0.0f)
	);
	const mat4 p = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

	const UniformBuffer ubo{ .mvp = p * m1 };

	updateUniformBuffer(imageIndex, &ubo, sizeof(ubo));

	fillCommandBuffers(imageIndex);

	const VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	// or even VERTEX_SHADER_STAGE

	const VkSubmitInfo si =
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vulkanRenderDevice.imageAvailableSemaphore,
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &vulkanRenderDevice.commandBuffers[imageIndex],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &vulkanRenderDevice.renderCompleteSemaphore
	};

	VK_CHECK(vkQueueSubmit(vulkanRenderDevice.graphicsQueue, 1, &si, nullptr));

	const VkPresentInfoKHR pi =
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vulkanRenderDevice.renderCompleteSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &vulkanRenderDevice.swapchain,
		.pImageIndices = &imageIndex
	};

	VK_CHECK(vkQueuePresentKHR(vulkanRenderDevice.graphicsQueue, &pi));
	VK_CHECK(vkDeviceWaitIdle(vulkanRenderDevice.device));

	return true;
}

int main()
{
	glslang_initialize_process();

	volkInitialize();

	if (!glfwInit())
		exit(EXIT_FAILURE);

	if (!glfwVulkanSupported())
		exit(EXIT_FAILURE);

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "VulkanApp", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwSetKeyCallback(
		window,
		[](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
				glfwSetWindowShouldClose(window, GLFW_TRUE);
		}
	);

	initVulkan();

	while (!glfwWindowShouldClose(window))
	{
		drawOverlay();
		glfwPollEvents();
	}

	terminateVulkan();
	glfwTerminate();
	glslang_finalize_process();

	return 0;
}