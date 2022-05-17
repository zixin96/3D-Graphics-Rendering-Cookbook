#define VK_NO_PROTOTYPES
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <deque>
#include <memory>
#include <limits>

#include "shared/Utils.h"
#include "shared/UtilsMath.h"
#include "shared/UtilsFPS.h"
#include "shared/UtilsVulkan.h"
#include "shared/vkRenderers/VulkanCanvas.h"
#include "shared/vkRenderers/VulkanCube.h"
#include "shared/vkRenderers/VulkanImGui.h"
#include "shared/vkRenderers/VulkanClear.h"
#include "shared/vkRenderers/VulkanFinish.h"
#include "shared/vkRenderers/VulkanModelRenderer.h"
#include "shared/Camera.h"
#include "shared/EasyProfilerWrapper.h"
#include "shared/Graph.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
using glm::mat4;
using glm::vec3;
using glm::vec4;

const uint32_t kScreenWidth = 1000;
const uint32_t kScreenHeight = 1000;

GLFWwindow* window;

// declare a Vulkan instance and render device objects
VulkanInstance vk;
VulkanRenderDevice vkDev;

// declare all our "layer" renderers
std::unique_ptr<ImGuiRenderer> imgui;
std::unique_ptr<ModelRenderer> modelRenderer;
std::unique_ptr<CubeRenderer> cubeRenderer;
std::unique_ptr<VulkanCanvas> canvas;
std::unique_ptr<VulkanCanvas> canvas2d;
std::unique_ptr<VulkanClear> clear;
std::unique_ptr<VulkanFinish> finish;

// create an FPS counter and charts (graphs)
FramesPerSecondCounter fpsCounter(0.02f);
LinearGraph fpsGraph;
LinearGraph sineGraph(4096);


struct MouseState
{
	glm::vec2 pos = glm::vec2(0.0f);
	bool pressedLeft = false;
} mouseState;

// All camera-related objects should be defined here:
glm::vec3 cameraPos(0.0f, 0.0f, 0.0f);
glm::vec3 cameraAngles(-45.0f, 0.0f, 0.0f);
CameraPositioner_FirstPerson positioner_firstPerson(cameraPos, vec3(0.0f, 0.0f, -1.0f), vec3(0.0f, 1.0f, 0.0f));
CameraPositioner_MoveTo positioner_moveTo(cameraPos, cameraAngles);
Camera camera = Camera(positioner_firstPerson);

// ImGUI stuff
const char* cameraType = "FirstPerson";
const char* comboBoxItems[] = {"FirstPerson", "MoveTo"};
const char* currentComboBoxItem = cameraType;

/**
 * \brief Initializes all necessary Vulkan components 
 * \return true if successful, otherwise, it will abort with EXIT_FAILURE
 */
bool initVulkan()
{
	EASY_FUNCTION();

	// initializes VkInstance inside VulkanInstance struct
	createInstance(&vk.instance);

	// initializes VkDebugUtilsMessengerEXT and VkDebugReportCallbackEXT inside VulkanInstance struct
	if (!setupDebugCallbacks(vk.instance, &vk.messenger, &vk.reportCallback))
		exit(EXIT_FAILURE);

	// initializes VkSurfaceKHR inside VulkanInstance struct
	if (glfwCreateWindowSurface(vk.instance, window, nullptr, &vk.surface))
		exit(EXIT_FAILURE);

	// initializes VulkanRenderDevice
	if (!initVulkanRenderDevice(vk,
	                            vkDev,
	                            kScreenWidth,
	                            kScreenHeight,
	                            isDeviceSuitable,
	                            {.geometryShader = VK_TRUE}))
	{
		exit(EXIT_FAILURE);
	}

	imgui = std::make_unique<ImGuiRenderer>(vkDev);

	// modelRenderer is initialized before other layers since it contains a depth buffer
	modelRenderer = std::make_unique<ModelRenderer>(vkDev,
	                                                "data/rubber_duck/scene.gltf",
	                                                "data/ch2_sample3_STB.jpg",
	                                                (uint32_t)sizeof(glm::mat4));

	cubeRenderer = std::make_unique<CubeRenderer>(vkDev,
	                                              modelRenderer->getDepthTexture(),
	                                              "data/piazza_bologni_1k.hdr");

	clear = std::make_unique<VulkanClear>(vkDev, modelRenderer->getDepthTexture());
	finish = std::make_unique<VulkanFinish>(vkDev, modelRenderer->getDepthTexture());

	// The canvas2d object takes an empty depth texture to disable depth testing
	canvas2d = std::make_unique<VulkanCanvas>(vkDev, VulkanImage{.image = VK_NULL_HANDLE, .imageView = VK_NULL_HANDLE});

	canvas = std::make_unique<VulkanCanvas>(vkDev, modelRenderer->getDepthTexture());

	return true;
}

void terminateVulkan()
{
	canvas = nullptr;
	canvas2d = nullptr;
	finish = nullptr;
	clear = nullptr;
	cubeRenderer = nullptr;
	modelRenderer = nullptr;
	imgui = nullptr;
	destroyVulkanRenderDevice(vkDev);
	destroyVulkanInstance(vk);
}

void reinitCamera()
{
	// 3D camera selection 

	if (!strcmp(cameraType, "FirstPerson"))
	{
		camera = Camera(positioner_firstPerson);
	}
	else
	{
		if (!strcmp(cameraType, "MoveTo"))
		{
			positioner_moveTo.setDesiredPosition(cameraPos);
			positioner_moveTo.setDesiredAngles(cameraAngles.x, cameraAngles.y, cameraAngles.z);
			camera = Camera(positioner_moveTo);
		}
	}
}

void renderGUI(uint32_t imageIndex)
{
	// This function contains all the Dear ImGui user interface rendering for the specified swapchain image

	EASY_FUNCTION();

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)width, (float)height);
	ImGui::NewFrame();

	// Render the FPS counter in a borderless ImGui window
	const ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoInputs |
		ImGuiWindowFlags_NoBackground;
	ImGui::SetNextWindowPos(ImVec2(0, 0));
	ImGui::Begin("Statistics", nullptr, flags);
	ImGui::Text("FPS: %.2f", fpsCounter.getFPS());
	ImGui::End();

	// Render the camera controls window
	ImGui::Begin("Camera Control", nullptr);
	{
		if (ImGui::BeginCombo("##combo", currentComboBoxItem))
		// The second parameter is the label previewed before opening the combo.
		{
			for (int n = 0; n < IM_ARRAYSIZE(comboBoxItems); n++)
			{
				const bool isSelected = (currentComboBoxItem == comboBoxItems[n]);

				if (ImGui::Selectable(comboBoxItems[n], isSelected))
					currentComboBoxItem = comboBoxItems[n];

				if (isSelected)
					ImGui::SetItemDefaultFocus();
				// You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)
			}
			ImGui::EndCombo();
		}

		if (!strcmp(cameraType, "MoveTo"))
		{
			// draw the ImGui sliders to select the camera position coordinates and orientation angles
			if (ImGui::SliderFloat3("Position", glm::value_ptr(cameraPos), -10.0f, +10.0f))
				positioner_moveTo.setDesiredPosition(cameraPos);
			if (ImGui::SliderFloat3("Pitch/Pan/Roll", glm::value_ptr(cameraAngles), -90.0f, +90.0f))
				positioner_moveTo.setDesiredAngles(cameraAngles);
		}

		if (currentComboBoxItem && strcmp(currentComboBoxItem, cameraType))
		{
			printf("Selected new camera type: %s\n", currentComboBoxItem);
			cameraType = currentComboBoxItem;
			// Reinitialize the camera if the camera mode has changed
			reinitCamera();
		}
	}

	// Finalize the ImGui rendering
	ImGui::End();
	ImGui::Render();

	// update the Vulkan buffers before issuing any Vulkan drawing commands:
	imgui->updateBuffers(vkDev, imageIndex, ImGui::GetDrawData());
}

void update3D(uint32_t imageIndex)
{
	// this function calculates the appropriate view and projection matrices for all objects and updates uniform buffers

	int width, height;
	glfwGetFramebufferSize(window, &width, &height);
	const float ratio = width / (float)height;

	const mat4 m1 = glm::rotate(
		glm::translate(mat4(1.0f), vec3(0.f, 0.5f, -1.5f)) * glm::rotate(mat4(1.f), glm::pi<float>(), vec3(1, 0, 0)),
		(float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
	const mat4 p = glm::perspective(45.0f, ratio, 0.1f, 1000.0f);

	const mat4 view = camera.getViewMatrix();
	const mat4 mtx = p * view * m1;

	{
		EASY_BLOCK("UpdateUniformBuffers");
		modelRenderer->updateUniformBuffer(vkDev, imageIndex, glm::value_ptr(mtx), sizeof(mat4));
		canvas->updateUniformBuffer(vkDev, p * view, 0.0f, imageIndex);
		canvas2d->updateUniformBuffer(vkDev, glm::ortho(0, 1, 1, 0), 0.0f, imageIndex);
		cubeRenderer->updateUniformBuffer(vkDev, imageIndex, p * view * m1);
		EASY_END_BLOCK;
	}
}

void update2D(uint32_t imageIndex)
{
	// similar to update3D, but for the user interface and onscreen graph

	canvas2d->clear();
	// render both charts using VulkanCanvas 
	sineGraph.renderGraph(*canvas2d.get(), vec4(0.0f, 1.0f, 0.0f, 1.0f));
	fpsGraph.renderGraph(*canvas2d.get());
	canvas2d->updateBuffer(vkDev, imageIndex);
}

void composeFrame(uint32_t imageIndex, const std::vector<RendererBase*>& renderers)
{
	// all the 2D, 3D, and user interface rendering data is updated
	update3D(imageIndex);
	renderGUI(imageIndex);
	update2D(imageIndex);

	EASY_BLOCK("FillCommandBuffers");

	// begin to fill a new command buffer by iterating all the layer renderers and
	// calling their fillCommandBuffer() virtual function

	VkCommandBuffer commandBuffer = vkDev.commandBuffers[imageIndex];

	const VkCommandBufferBeginInfo bi =
	{
		.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.pNext = nullptr,
		.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
		.pInheritanceInfo = nullptr
	};

	VK_CHECK(vkBeginCommandBuffer(commandBuffer, &bi));

	for (auto& r : renderers)
		r->fillCommandBuffer(commandBuffer, imageIndex);

	VK_CHECK(vkEndCommandBuffer(commandBuffer));

	EASY_END_BLOCK;
}

bool drawFrame(const std::vector<RendererBase*>& renderers)
{
	EASY_FUNCTION();

	uint32_t imageIndex = 0;
	VkResult result = vkAcquireNextImageKHR(vkDev.device,
	                                        vkDev.swapchain,
	                                        0,
	                                        vkDev.imageAvailableSemaphore,
	                                        VK_NULL_HANDLE,
	                                        &imageIndex);
	VK_CHECK(vkResetCommandPool(vkDev.device, vkDev.commandPool, 0));

	// Here, if the next swapchain image is not yet available, we should return and skip this
	// frame. It might just be that our GPU is rendering frames slower than we are filling in the command buffers
	if (result != VK_SUCCESS) return false;

	composeFrame(imageIndex, renderers);

	const VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	// or even VERTEX_SHADER_STAGE

	// Submit the command buffer into the Vulkan graphics queue

	const VkSubmitInfo si =
	{
		.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vkDev.imageAvailableSemaphore,
		.pWaitDstStageMask = waitStages,
		.commandBufferCount = 1,
		.pCommandBuffers = &vkDev.commandBuffers[imageIndex],
		.signalSemaphoreCount = 1,
		.pSignalSemaphores = &vkDev.renderCompleteSemaphore
	};

	{
		EASY_BLOCK("vkQueueSubmit", profiler::colors::Magenta);
		VK_CHECK(vkQueueSubmit(vkDev.graphicsQueue, 1, &si, nullptr));
		EASY_END_BLOCK;
	}

	// Present the results on the screen

	const VkPresentInfoKHR pi =
	{
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.waitSemaphoreCount = 1,
		.pWaitSemaphores = &vkDev.renderCompleteSemaphore,
		.swapchainCount = 1,
		.pSwapchains = &vkDev.swapchain,
		.pImageIndices = &imageIndex
	};

	{
		EASY_BLOCK("vkQueuePresentKHR", profiler::colors::Magenta);
		VK_CHECK(vkQueuePresentKHR(vkDev.graphicsQueue, &pi));
		EASY_END_BLOCK;
	}

	// Wait for the GPU to finish rendering
	{
		EASY_BLOCK("vkDeviceWaitIdle", profiler::colors::Red);
		VK_CHECK(vkDeviceWaitIdle(vkDev.device));
		EASY_END_BLOCK;
	}

	return true;
}

int main()
{
	// initialize profiler
	EASY_PROFILER_ENABLE;
	EASY_MAIN_THREAD;

	// initializes the glslang compiler
	glslang_initialize_process();

	// initializes the Volk library
	volkInitialize();

	// initializes GLFW
	if (!glfwInit())
		exit(EXIT_FAILURE);

	// is Vulkan "minimally" available? 
	if (!glfwVulkanSupported())
		exit(EXIT_FAILURE);

	// disable any OpenGL context creation
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	// disable resized windows
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);

	// set up the data structures that are needed to sustain an ImGui context
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();

	// create GLFW window
	window = glfwCreateWindow(kScreenWidth, kScreenHeight, "VulkanApp", nullptr, nullptr);
	if (!window)
	{
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	// sets the cursor position callback
	glfwSetCursorPosCallback(
		window,
		[](auto* window, double x, double y)
		{
			// route the normalized GLFW mouse position into mouseState struct (so that we can look around using mouse)
			// which will later be passed into Camera::update 

			// Notice that we convert window pixel coordinates into normalized [0,1] coordinates
			int width, height;
			glfwGetFramebufferSize(window, &width, &height);
			mouseState.pos.x = static_cast<float>(x / width);
			mouseState.pos.y = static_cast<float>(y / height);

			// We must route GLFW mouse position into ImGui so that we can interact IMGUI window with the mouse
			ImGui::GetIO().MousePos = ImVec2((float)x, (float)y);
		}
	);

	// sets the mouse button callback
	glfwSetMouseButtonCallback(
		window,
		[](auto* window, int button, int action, int mods)
		{
			// route the mouse button events into ImGui so that we can interact IMGUI window with the mouse clicks 
			auto& io = ImGui::GetIO();
			// TODO: If right clicking doesn't work, check back here.
			// const int idx = button == GLFW_MOUSE_BUTTON_LEFT ? 0 : button == GLFW_MOUSE_BUTTON_RIGHT ? 1 : 2;
			const int idx = button == GLFW_MOUSE_BUTTON_LEFT ? 0 : button == GLFW_MOUSE_BUTTON_RIGHT ? 2 : 1;
			io.MouseDown[idx] = action == GLFW_PRESS;

			// route the mouse button event into mouseState struct (so that we can look around using mouse)
			if (button == GLFW_MOUSE_BUTTON_LEFT)
			{
				mouseState.pressedLeft = action == GLFW_PRESS;
			}
		}
	);

	// sets the key callback
	glfwSetKeyCallback(
		window,
		[](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			const bool pressed = action != GLFW_RELEASE;
			if (key == GLFW_KEY_ESCAPE && pressed)
				glfwSetWindowShouldClose(window, GLFW_TRUE);

			// handles keyboard input for camera movement
			if (key == GLFW_KEY_W)
				positioner_firstPerson.movement_.forward_ = pressed;
			if (key == GLFW_KEY_S)
				positioner_firstPerson.movement_.backward_ = pressed;
			if (key == GLFW_KEY_A)
				positioner_firstPerson.movement_.left_ = pressed;
			if (key == GLFW_KEY_D)
				positioner_firstPerson.movement_.right_ = pressed;
			if (key == GLFW_KEY_SPACE)
			{
				// reorient the camera up vector to the world (0, 1, 0) vector
				positioner_firstPerson.setUpVector(vec3(0.0f, 1.0f, 0.0f));
			}
		}
	);

	initVulkan();

	{
		canvas->plane3d(vec3(0, +1.5, 0), vec3(1, 0, 0), vec3(0, 0, 1), 40, 40, 10.0f, 10.0f, vec4(1, 0, 0, 1),
		                vec4(0, 1, 0, 1));

		for (size_t i = 0; i < vkDev.swapchainImages.size(); i++)
			canvas->updateBuffer(vkDev, i);
	}

	double timeStamp = glfwGetTime();
	float deltaSeconds = 0.0f;

	const std::vector<RendererBase*> renderers = {
		clear.get(),
		cubeRenderer.get(),
		modelRenderer.get(),
		canvas.get(),
		canvas2d.get(),
		imgui.get(),
		finish.get()
	};

	while (!glfwWindowShouldClose(window))
	{
		{
			EASY_BLOCK("UpdateCameraPositioners")
			positioner_firstPerson.update(deltaSeconds, mouseState.pos, mouseState.pressedLeft);
			positioner_moveTo.update(deltaSeconds, mouseState.pos, mouseState.pressedLeft);
			EASY_END_BLOCK;
		}

		const double newTimeStamp = glfwGetTime();
		deltaSeconds = static_cast<float>(newTimeStamp - timeStamp);
		timeStamp = newTimeStamp;

		// Once our frame composition is done, we can proceed with the frame rendering
		// the function is invoked from the main loop using the list of layer renderers
		const bool frameRendered = drawFrame(renderers);

		// add points to the graph
		if (fpsCounter.tick(deltaSeconds, frameRendered))
		{
			fpsGraph.addPoint(fpsCounter.getFPS());
		}
		sineGraph.addPoint((float)sin(glfwGetTime() * 10.0));

		{
			EASY_BLOCK("PollEvents");
			glfwPollEvents();
			EASY_END_BLOCK;
		}
	}

	ImGui::DestroyContext();

	terminateVulkan();
	glfwTerminate();
	glslang_finalize_process();

	PROFILER_DUMP("profiling.prof");

	return 0;
}
