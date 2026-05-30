
#pragma once

#include "game_platform.h"
#include "linux.h"

#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

#define VULKAN_INSTANCE_FUNCTIONS                                     \
        INSTANCE_FUNCTION(vkGetInstanceProcAddr);                     \
        INSTANCE_FUNCTION(vkDestroyInstance);                         \
        INSTANCE_FUNCTION(vkEnumeratePhysicalDevices);                \
        INSTANCE_FUNCTION(vkGetPhysicalDeviceProperties);             \
        INSTANCE_FUNCTION(vkGetPhysicalDeviceFeatures);               \
        INSTANCE_FUNCTION(vkGetPhysicalDeviceQueueFamilyProperties);  \
        INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceSupportKHR);      \
        INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceCapabilitiesKHR); \
        INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfaceFormatsKHR);      \
        INSTANCE_FUNCTION(vkGetPhysicalDeviceSurfacePresentModesKHR); \
        INSTANCE_FUNCTION(vkGetPhysicalDeviceMemoryProperties);       \
        INSTANCE_FUNCTION(vkCreateDevice);                            \
        INSTANCE_FUNCTION(vkGetDeviceProcAddr);                       \
        INSTANCE_FUNCTION(vkDestroySurfaceKHR);

#define VULKAN_DEVICE_FUNCTIONS                         \
        DEVICE_FUNCTION(vkGetDeviceQueue);              \
        DEVICE_FUNCTION(vkCreateSwapchainKHR);          \
        DEVICE_FUNCTION(vkGetSwapchainImagesKHR);       \
        DEVICE_FUNCTION(vkCreateImageView);             \
        DEVICE_FUNCTION(vkCreateImage);                 \
        DEVICE_FUNCTION(vkGetImageMemoryRequirements);  \
        DEVICE_FUNCTION(vkBindImageMemory);             \
        DEVICE_FUNCTION(vkCreateSampler);               \
        DEVICE_FUNCTION(vkCreateDescriptorSetLayout);   \
        DEVICE_FUNCTION(vkCreateDescriptorPool);        \
        DEVICE_FUNCTION(vkAllocateDescriptorSets);      \
        DEVICE_FUNCTION(vkUpdateDescriptorSets);        \
        DEVICE_FUNCTION(vkCmdPipelineBarrier);          \
        DEVICE_FUNCTION(vkCmdCopyBufferToImage);        \
        DEVICE_FUNCTION(vkCmdBindDescriptorSets);       \
        DEVICE_FUNCTION(vkCreateCommandPool);           \
        DEVICE_FUNCTION(vkAllocateCommandBuffers);      \
        DEVICE_FUNCTION(vkCreateSemaphore);             \
        DEVICE_FUNCTION(vkCreateFence);                 \
        DEVICE_FUNCTION(vkCreateRenderPass);            \
        DEVICE_FUNCTION(vkCreateFramebuffer);           \
        DEVICE_FUNCTION(vkWaitForFences);               \
        DEVICE_FUNCTION(vkResetFences);                 \
        DEVICE_FUNCTION(vkAcquireNextImageKHR);         \
        DEVICE_FUNCTION(vkResetCommandBuffer);          \
        DEVICE_FUNCTION(vkBeginCommandBuffer);          \
        DEVICE_FUNCTION(vkEndCommandBuffer);            \
        DEVICE_FUNCTION(vkCmdBeginRenderPass);          \
        DEVICE_FUNCTION(vkCmdEndRenderPass);            \
        DEVICE_FUNCTION(vkQueueSubmit);                 \
        DEVICE_FUNCTION(vkQueuePresentKHR);             \
        DEVICE_FUNCTION(vkDeviceWaitIdle);              \
        DEVICE_FUNCTION(vkDestroySwapchainKHR);         \
        DEVICE_FUNCTION(vkDestroyImageView);            \
        DEVICE_FUNCTION(vkDestroyFramebuffer);          \
        DEVICE_FUNCTION(vkDestroySemaphore);            \
        DEVICE_FUNCTION(vkCreateBuffer);                \
        DEVICE_FUNCTION(vkGetBufferMemoryRequirements); \
        DEVICE_FUNCTION(vkAllocateMemory);              \
        DEVICE_FUNCTION(vkBindBufferMemory);            \
        DEVICE_FUNCTION(vkMapMemory);                   \
        DEVICE_FUNCTION(vkCreateShaderModule);          \
        DEVICE_FUNCTION(vkCreatePipelineLayout);        \
        DEVICE_FUNCTION(vkCreateGraphicsPipelines);     \
        DEVICE_FUNCTION(vkDestroyShaderModule);         \
        DEVICE_FUNCTION(vkCmdSetViewport);              \
        DEVICE_FUNCTION(vkCmdSetScissor);               \
        DEVICE_FUNCTION(vkCmdBindPipeline);             \
        DEVICE_FUNCTION(vkCmdBindVertexBuffers);        \
        DEVICE_FUNCTION(vkCmdDraw);

#define VULKAN_DEBUG_FUNCTIONS \
        DEBUG_FUNCTION(vkCreateDebugUtilsMessengerEXT);

#define VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT 8
#define FRAME_COUNT 3

typedef struct {
	u32 graphicsIndex;	
	u32 presentIndex;	
} VulkanQueueFamilies;

typedef struct {
	VkSwapchainKHR handle;
	VkFormat format;
	VkExtent2D extent;

	u32 imageCount;

	VkImage images[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
	VkImageView imageViews[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
	VkSemaphore renderFinishedSemaphore[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
} VulkanSwapchain;

typedef struct {
	VkCommandBuffer commandBuffer;
	VkSemaphore imageAvailableSemaphore;
	VkFence inFlightFence;
} VulkanFrameData;

typedef struct {
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkSurfaceKHR surface;

	VulkanQueueFamilies queueFamilies;
	VkQueue graphicsQueue;
	VkQueue presentQueue;
	
	VkDevice logicalDevice;
	VulkanSwapchain swapchain;

	VkCommandPool commandPool;
	VulkanFrameData frameData[FRAME_COUNT];

	VkPipeline pipeline;
	VkPipelineLayout pipelineLayout;

	u32 imageIndex;
	u32 frameIndex;

#if defined(DEBUG)
        VkDebugUtilsMessengerEXT debugMessenger;
#define DEBUG_FUNCTION(name) PFN_##name name
        VULKAN_DEBUG_FUNCTIONS
#undef DEBUG_FUNCTION
#endif

#define INSTANCE_FUNCTION(name) PFN_##name name
#define DEVICE_FUNCTION(name) PFN_##name name
        VULKAN_INSTANCE_FUNCTIONS
        VULKAN_DEVICE_FUNCTIONS
#undef INSTANCE_FUNCTION
#undef DEVICE_FUNCTION
} Vulkan;

void VulkanInitialize(Vulkan *vulkan, LinuxWayland *window);

bool VulkanFrameBegin(Vulkan *vulkan, LinuxWayland *window);

void VulkanFrameEnd(Vulkan *vulkan);
