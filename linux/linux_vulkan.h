#pragma once

#include "game_platform.h"
#include "linux.h"
#include <vulkan/vulkan_core.h>

#define VK_USE_PLATFORM_WAYLAND_KHR
#include <vulkan/vulkan.h>

#define VULKAN_INSTANCE_FUNCTIONS                                 \
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

#define VULKAN_DEVICE_FUNCTIONS                     \
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
    DEVICE_FUNCTION(vkDestroyCommandPool);          \
    DEVICE_FUNCTION(vkAllocateCommandBuffers);      \
    DEVICE_FUNCTION(vkCreateSemaphore);             \
    DEVICE_FUNCTION(vkFreeMemory);                  \
    DEVICE_FUNCTION(vkDestroyBuffer);               \
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
    DEVICE_FUNCTION(vkQueueWaitIdle);               \
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
    DEVICE_FUNCTION(vkUnmapMemory);                 \
    DEVICE_FUNCTION(vkCreateShaderModule);          \
    DEVICE_FUNCTION(vkCreatePipelineLayout);        \
    DEVICE_FUNCTION(vkCreateGraphicsPipelines);     \
    DEVICE_FUNCTION(vkDestroyShaderModule);         \
    DEVICE_FUNCTION(vkCmdSetViewport);              \
    DEVICE_FUNCTION(vkCmdSetScissor);               \
    DEVICE_FUNCTION(vkCmdBindPipeline);             \
    DEVICE_FUNCTION(vkCmdBindVertexBuffers);        \
    DEVICE_FUNCTION(vkCmdBeginRendering);           \
    DEVICE_FUNCTION(vkCmdEndRendering);             \
    DEVICE_FUNCTION(vkCmdPushConstants);            \
    DEVICE_FUNCTION(vkCmdDraw);

#define VULKAN_DEBUG_FUNCTIONS \
    DEBUG_FUNCTION(vkCreateDebugUtilsMessengerEXT);

#define VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT 8
#define FRAME_COUNT 3
#define MAX_TEXTURES 128

typedef struct {
    u32 graphics_index;
    u32 present_index;
} vulkan_queue_families;

typedef struct {
    VkSwapchainKHR handle;
    VkFormat format;
    VkExtent2D extent;

    u32 image_count;

    VkImage images[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
    VkImageView image_views[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
    VkSemaphore render_finished_semaphore[VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT];
} vulkan_swapchain;

typedef struct {
    VkImage handle;
    VkImageView view;
    VkDeviceMemory memory;
} vulkan_image;

typedef struct {
    VkBuffer handle;
    VkDeviceMemory memory;
    VkDeviceSize size;
} vulkan_buffer;

typedef struct {
    VkCommandBuffer command_buffer;
    VkSemaphore image_available_semaphore;
    VkFence in_flight_fence;

    vulkan_buffer uniform_buffer;
    u8 *uniform_data;
    u32 uniform_count;
} vulkan_frame_data;

typedef struct {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkSurfaceKHR surface;
    // NOTE: last VkResult
    VkResult result;

    vulkan_queue_families queue_families;
    VkQueue graphics_queue;
    VkQueue present_queue;

    VkDevice logical_device;
    vulkan_swapchain swapchain;

    VkCommandPool command_pool;
    vulkan_frame_data frame_data[FRAME_COUNT];

    VkPipeline pipeline;
    VkPipelineLayout pipeline_layout;

    VkDescriptorPool descriptor_pool;

    VkDescriptorSetLayout resource_descriptor_layout;
    VkDescriptorSetLayout sampler_descriptor_set_layout;
    VkDescriptorSet resource_descriptor_sets[FRAME_COUNT];
    VkDescriptorSet sampler_descriptor_sets[FRAME_COUNT];
    u32 uniform_stride;

    VkSampler texture_sampler;

    vulkan_buffer vertex_buffer;
    u8 *vertex_data;
    u32 vertex_capacity;
    u32 vertex_count;

    VkClearValue clear_color;

    u32 image_index;
    u32 frame_index;

    vulkan_image textures[MAX_TEXTURES];
    u32 texture_count;

    memory_stream *info_stream;
    memory_stream *error_stream;

#if defined(DEBUG)
    VkDebugUtilsMessengerEXT debug_messenger;
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
} vulkan;

bool vulkan_initialize(vulkan *vulkan, linux_wayland *window, memory_stream *info_stream, memory_stream *error_stream);

bool vulkan_frame_begin(vulkan *vulkan, linux_wayland *wayland, render_command_buffer *command_buffer);
bool vulkan_frame_end(vulkan *vulkan, render_command_buffer *command_buffer);

// NOTE: borrowed from
// https://github.com/khronos_group/vulkan-utility-libraries/blob/main/include/vulkan/vk_enum_string_helper.h

static inline const char *result_to_string(VkResult input_value) {
    switch ((VkResult)input_value) {
    case VK_ERROR_DEVICE_LOST:
        return "VK_ERROR_DEVICE_LOST";
    case VK_ERROR_EXTENSION_NOT_PRESENT:
        return "VK_ERROR_EXTENSION_NOT_PRESENT";
    case VK_ERROR_FEATURE_NOT_PRESENT:
        return "VK_ERROR_FEATURE_NOT_PRESENT";
    case VK_ERROR_FORMAT_NOT_SUPPORTED:
        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
    case VK_ERROR_FRAGMENTATION_EXT:
        return "VK_ERROR_FRAGMENTATION_EXT";
    case VK_ERROR_FRAGMENTED_POOL:
        return "VK_ERROR_FRAGMENTED_POOL";
    case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
        return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
    case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:
        return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
    case VK_ERROR_INCOMPATIBLE_DRIVER:
        return "VK_ERROR_INCOMPATIBLE_DRIVER";
    case VK_ERROR_INITIALIZATION_FAILED:
        return "VK_ERROR_INITIALIZATION_FAILED";
    case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:
        return "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT";
    case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
        return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
    case VK_ERROR_INVALID_EXTERNAL_HANDLE:
        return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
    case VK_ERROR_INVALID_SHADER_NV:
        return "VK_ERROR_INVALID_SHADER_NV";
    case VK_ERROR_LAYER_NOT_PRESENT:
        return "VK_ERROR_LAYER_NOT_PRESENT";
    case VK_ERROR_MEMORY_MAP_FAILED:
        return "VK_ERROR_MEMORY_MAP_FAILED";
    case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
        return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
    case VK_ERROR_NOT_PERMITTED_EXT:
        return "VK_ERROR_NOT_PERMITTED_EXT";
    case VK_ERROR_OUT_OF_DATE_KHR:
        return "VK_ERROR_OUT_OF_DATE_KHR";
    case VK_ERROR_OUT_OF_DEVICE_MEMORY:
        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
    case VK_ERROR_OUT_OF_HOST_MEMORY:
        return "VK_ERROR_OUT_OF_HOST_MEMORY";
    case VK_ERROR_OUT_OF_POOL_MEMORY:
        return "VK_ERROR_OUT_OF_POOL_MEMORY";
    case VK_ERROR_SURFACE_LOST_KHR:
        return "VK_ERROR_SURFACE_LOST_KHR";
    case VK_ERROR_TOO_MANY_OBJECTS:
        return "VK_ERROR_TOO_MANY_OBJECTS";
    case VK_ERROR_VALIDATION_FAILED_EXT:
        return "VK_ERROR_VALIDATION_FAILED_EXT";
    case VK_EVENT_RESET:
        return "VK_EVENT_RESET";
    case VK_EVENT_SET:
        return "VK_EVENT_SET";
    case VK_INCOMPLETE:
        return "VK_INCOMPLETE";
    case VK_NOT_READY:
        return "VK_NOT_READY";
    case VK_SUBOPTIMAL_KHR:
        return "VK_SUBOPTIMAL_KHR";
    case VK_SUCCESS:
        return "VK_SUCCESS";
    case VK_TIMEOUT:
        return "VK_TIMEOUT";
    default:
        return "unhandled VkResult";
    }
}

#define THIS_MUST_SUCCEED(vulkan, call)                                                                                           \
    do {                                                                                                                          \
        VkResult vulkan_result = (call);                                                                                          \
        if (vulkan_result < 0) {                                                                                                  \
            memory_stream_write_string_format((vulkan)->error_stream, "error: %s: %s\n", #call, result_to_string(vulkan_result)); \
            ASSERT(0)                                                                                                             \
        }                                                                                                                         \
    } while (0)
