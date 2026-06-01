// TODO:
// * Refactor and improve error handling to be more like in Win32.
//   - [x] Use bool for return type when a function can fail
//   - [x] Check all Vulkan API calls
//         - [ ] Print all VkResult's
// * Add support for Windows. So on Windows there must be a choice between DirectX and Vulkan, and on Linux, well, nothing would change. Maybe there will be OpenGL later.
// * I think I noticed that some things between Vulkan and DirectX are repeated. Maybe we can put them somewhere like game_renderer.c?
// * Make all `fprintf`s calls (error printing basically) consistent.
#include "linux_vulkan.h"
#include "game_platform.h"
#include "game_types.h"
#include "linux.h"

#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>

// NOTE: Compiled shaders.
#include "BasicGeometry.VS.spv.h"
#include "BasicGeometry.PS.spv.h"

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;

    u32 formatCount;
    VkSurfaceFormatKHR formats[32];

    u32 presentModeCount;
    VkPresentModeKHR presentModes[8];
} VulkanSwapchainSupport;

static u32 VulkanMemoryTypeFind(Vulkan *vulkan, u32 typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vulkan->vkGetPhysicalDeviceMemoryProperties(vulkan->physicalDevice, &memoryProperties);

    for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return 0;
}

static VulkanBuffer VulkanBufferHostVisibleCreate(Vulkan *vulkan, VkDeviceSize size, VkBufferUsageFlags flags, void **mapped) {
    VulkanBuffer result = {
        .size = size,
    };

    VkBufferCreateInfo bufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = flags,
        .size = size,
    };

    if (vulkan->vkCreateBuffer(vulkan->logicalDevice, &bufferCreateInfo, 0, &result.handle) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateBuffer\n");

        return result;
    }

    VkMemoryRequirements memoryRequirements;
    vulkan->vkGetBufferMemoryRequirements(vulkan->logicalDevice, result.handle, &memoryRequirements);

    VkMemoryAllocateInfo allocationInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = VulkanMemoryTypeFind(vulkan, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };

    if (vulkan->vkAllocateMemory(vulkan->logicalDevice, &allocationInfo, 0, &result.memory) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkAllocateMemory\n");

        return result;
    }

    if (vulkan->vkBindBufferMemory(vulkan->logicalDevice, result.handle, result.memory, 0) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkBindBufferMemory\n");

        return result;
    }

    if (vulkan->vkMapMemory(vulkan->logicalDevice, result.memory, 0, result.size, 0, mapped) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkMapMemory\n");

        return result;
    }

    return result;
}

static void VulkanBufferHostVisibleDestroy(Vulkan *vulkan, VulkanBuffer buffer) {
    vulkan->vkUnmapMemory(vulkan->logicalDevice, buffer.memory);
    vulkan->vkFreeMemory(vulkan->logicalDevice, buffer.memory, 0);
    vulkan->vkDestroyBuffer(vulkan->logicalDevice, buffer.handle, 0);
}

static void VulkanInstanceCreate(Vulkan *vulkan, PFN_vkCreateInstance vkCreateInstance, const char *platformExtension) {
    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        platformExtension,
#if defined(DEBUG)
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

#if defined(DEBUG)
    fprintf(stderr, "Validation layers will be enabled\n");

    const char *layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };
#endif

    VkApplicationInfo applicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Game",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Miguel Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo instanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &applicationInfo,
        .enabledExtensionCount = ArrayCount(extensions),
        .ppEnabledExtensionNames = extensions,
#if defined(DEBUG)
        .enabledLayerCount = ArrayCount(layers),
        .ppEnabledLayerNames = layers,
#endif
    };

    if (vkCreateInstance(&instanceCreateInfo, 0, &vulkan->instance) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to create vulkan instance.\n");

        return;
    }
}

#if defined(DEBUG)

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *userData) {
    Unused(severity);
    Unused(type);
    Unused(userData);
    fprintf(stderr, "%s\n", data->pMessage);

    return VK_FALSE;
}

static void VulkanDebugMessengerCreate(Vulkan *vulkan) {
    VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = VulkanDebugMessengerCallback,
    };

    if (vulkan->vkCreateDebugUtilsMessengerEXT(vulkan->instance, &debugMessengerCreateInfo, 0, &vulkan->debugMessenger) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateDebugUtilsMessengerEXT\n");

        return;
    }
}

#endif

static bool VulkanQueueFamiliesComplete(Vulkan *vulkan) {
    return (vulkan->queueFamilies.graphicsIndex != 0xffffffff && vulkan->queueFamilies.presentIndex != 0xffffffff);
}

static void VulkanQueueFamiliesFind(Vulkan *vulkan, VkPhysicalDevice physicalDevice) {
    vulkan->queueFamilies.graphicsIndex = 0xffffffff;
    vulkan->queueFamilies.presentIndex = 0xffffffff;

    u32 count = 0;
    vulkan->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, 0);

    count = Min(count, 16);

    VkQueueFamilyProperties families[16];
    vulkan->vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, families);

    for (u32 i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            vulkan->queueFamilies.graphicsIndex = i;
        }

        VkBool32 presentSupport = false;
        vulkan->vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, vulkan->surface, &presentSupport);
        if (presentSupport) {
            vulkan->queueFamilies.presentIndex = i;
        }

        if (VulkanQueueFamiliesComplete(vulkan)) {
            break;
        }
    }
}

static bool VulkanPhysicalDeviceSuitable(Vulkan *vulkan, VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceProperties properties;
    vulkan->vkGetPhysicalDeviceProperties(physicalDevice, &properties);

    VkPhysicalDeviceFeatures features;
    vulkan->vkGetPhysicalDeviceFeatures(physicalDevice, &features);
    VulkanQueueFamiliesFind(vulkan, physicalDevice);
    return (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && VulkanQueueFamiliesComplete(vulkan));
}

static bool VulkanPhysicalDevicePick(Vulkan *vulkan) {
    u32 count = 0;
    if (vulkan->vkEnumeratePhysicalDevices(vulkan->instance, &count, 0) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkEnumeratePhysicalDevices\n");

        return false;
    };

    count = Min(count, 8);

    VkPhysicalDevice devices[8];
    if (vulkan->vkEnumeratePhysicalDevices(vulkan->instance, &count, devices) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkEnumeratePhysicalDevices\n");

        return false;
    };

    for (u32 i = 0; i < count; i++) {
        if (VulkanPhysicalDeviceSuitable(vulkan, devices[i])) {
            vulkan->physicalDevice = devices[i];

            break;
        }
    }

    if (!vulkan->physicalDevice) {
        fprintf(stderr, "WARNING: I did not find a dedicated GPU!\n");
        vulkan->physicalDevice = devices[0];
    }

    if (!vulkan->physicalDevice) {
        fprintf(stderr, "ERROR: Your GPU is not supported by vulkan!\n");

        return false;
    }

    VkPhysicalDeviceProperties properties;
    vulkan->vkGetPhysicalDeviceProperties(vulkan->physicalDevice, &properties);
    fprintf(stderr, "GPU: %s\n", properties.deviceName);

    return true;
}

static bool VulkanLogicalDeviceCreate(Vulkan *vulkan) {
    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    f32 queuePriority = 1.0f;

    u32 uniqueIndices[2];
    u32 uniqueCount = 0;

    uniqueIndices[uniqueCount++] = vulkan->queueFamilies.graphicsIndex;
    if (vulkan->queueFamilies.presentIndex != vulkan->queueFamilies.graphicsIndex) {
        fprintf(stderr, "Graphics and present queue are from diffrent families.\n");

        uniqueIndices[uniqueCount++] = vulkan->queueFamilies.presentIndex;
    }

    VkDeviceQueueCreateInfo queueCreateInfos[2];

    for (u32 i = 0; i < uniqueCount; i++) {
        queueCreateInfos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = uniqueIndices[i],
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features vulkan12Features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &dynamicRendering,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
    };

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &vulkan12Features,
        .pQueueCreateInfos = queueCreateInfos,
        .queueCreateInfoCount = uniqueCount,
        .enabledExtensionCount = ArrayCount(extensions),
        .ppEnabledExtensionNames = extensions,
    };

    if (vulkan->vkCreateDevice(vulkan->physicalDevice, &deviceCreateInfo, 0, &vulkan->logicalDevice) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateDevice\n");

        return false;
    }

    return true;
}

static VulkanSwapchainSupport VulkanSwapchainSupportQuery(Vulkan *vulkan) {
    VulkanSwapchainSupport result = {0};

    if (vulkan->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan->physicalDevice, vulkan->surface, &result.capabilities) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkGetPhysicalDeviceSurfaceCapabilitiesKHR\n");

        return result;
    };
    if (vulkan->vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physicalDevice, vulkan->surface, &result.formatCount, 0) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkGetPhysicalDeviceSurfaceFormatsKHR\n");

        return result;
    };
    if (!result.formatCount) {
        fprintf(stderr, "ERROR: Failed to get vulkan surface formats.\n");

        return result;
    }

    // NOTE: There will be a segmentation fault if we don't clamp this because drivers can sometimes return just a lot of them.
    result.formatCount = Min(result.formatCount, 32);
    if (vulkan->vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physicalDevice, vulkan->surface, &result.formatCount, result.formats) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkGetPhysicalDeviceSurfaceFormatsKHR\n");

        return result;
    };

    if (vulkan->vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->physicalDevice, vulkan->surface, &result.presentModeCount, 0) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkGetPhysicalDeviceSurfacePresentModesKHR\n");

        return result;
    };
    if (!result.presentModeCount) {
        fprintf(stderr, "ERROR: Failed to get vulkan surface present modes.\n");

        return result;
    }
    result.presentModeCount = Min(result.presentModeCount, 8);
    if (vulkan->vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->physicalDevice, vulkan->surface, &result.presentModeCount, result.presentModes) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkGetPhysicalDeviceSurfacePresentModesKHR\n");

        return result;
    };

    return result;
}

static VkSurfaceFormatKHR VulkanSurfaceFormatSelect(VulkanSwapchainSupport *support) {
    for (u32 i = 0; i < support->formatCount; i++) {
        VkSurfaceFormatKHR format = support->formats[i];
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return support->formats[0];
}

static VkPresentModeKHR VulkanPresentModeSelect(VulkanSwapchainSupport *support) {
    for (u32 i = 0; i < support->presentModeCount; i++) {
        VkPresentModeKHR presentMode = support->presentModes[i];
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D VulkanExtentSelect(VulkanSwapchainSupport *support, u32 width, u32 height) {
    if (support->capabilities.currentExtent.width != 0xffffffff) {
        return support->capabilities.currentExtent;
    }

    VkExtent2D extent = {width, height};

    VkExtent2D min = support->capabilities.minImageExtent;
    VkExtent2D max = support->capabilities.maxImageExtent;

    if (extent.width < min.width) {
        extent.width = min.width;
    }
    if (extent.width > max.width) {
        extent.width = max.width;
    }
    if (extent.height < min.height) {
        extent.height = min.height;
    }
    if (extent.height > max.height) {
        extent.height = max.height;
    }

    return extent;
}

static bool VulkanSwapchainCreate(Vulkan *vulkan, u32 windowWidth, u32 windowHeight) {
    VulkanSwapchainSupport support = VulkanSwapchainSupportQuery(vulkan);
    VkSurfaceFormatKHR surfaceFormat = VulkanSurfaceFormatSelect(&support);
    VkPresentModeKHR presentMode = VulkanPresentModeSelect(&support);

    VkExtent2D extent = VulkanExtentSelect(&support, (u32)windowWidth, (u32)windowHeight);

    u32 imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount) {
        imageCount = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vulkan->surface,
        .minImageCount = imageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = support.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    u32 queueFamilyIndices[] = {
        vulkan->queueFamilies.graphicsIndex,
        vulkan->queueFamilies.presentIndex,
    };

    if (vulkan->queueFamilies.graphicsIndex != vulkan->queueFamilies.presentIndex) {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    };

    VulkanSwapchain *swapchain = &vulkan->swapchain;
    if (vulkan->vkCreateSwapchainKHR(vulkan->logicalDevice, &swapchainCreateInfo, 0, &swapchain->handle) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateSwapchainKHR\n");

        return false;
    }
    swapchain->format = surfaceFormat.format;
    swapchain->extent = extent;

    if (vulkan->vkGetSwapchainImagesKHR(vulkan->logicalDevice, swapchain->handle, &swapchain->imageCount, 0) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkGetSwapchainImagesKHR\n");

        return false;
    };
    assert(swapchain->imageCount <= VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT);
    if (vulkan->vkGetSwapchainImagesKHR(vulkan->logicalDevice, swapchain->handle, &swapchain->imageCount, swapchain->images) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkGetSwapchainImagesKHR\n");

        return false;
    };

    for (u32 i = 0; i < swapchain->imageCount; i++) {
        VkImageViewCreateInfo imageViewCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchain->images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = vulkan->swapchain.format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .layerCount = 1,
                .levelCount = 1,
            },
        };

        if (vulkan->vkCreateImageView(vulkan->logicalDevice, &imageViewCreateInfo, 0, &swapchain->imageViews[i]) != VK_SUCCESS) {
            fprintf(stderr, "ERROR: vkCreateImageView\n");

            return false;
        }

        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        if (vulkan->vkCreateSemaphore(vulkan->logicalDevice, &semaphoreCreateInfo, 0, &swapchain->renderFinishedSemaphore[i]) != VK_SUCCESS) {
            fprintf(stderr, "ERROR: vkCreateSemaphore\n");

            return false;
        }
    }

    return true;
}

static void VulkanSwapchainDestroy(Vulkan *vulkan) {
    for (u32 i = 0; i < vulkan->swapchain.imageCount; i++) {
        vulkan->vkDestroyImageView(vulkan->logicalDevice, vulkan->swapchain.imageViews[i], 0);
        vulkan->vkDestroySemaphore(vulkan->logicalDevice, vulkan->swapchain.renderFinishedSemaphore[i], 0);
    }

    vulkan->vkDestroySwapchainKHR(vulkan->logicalDevice, vulkan->swapchain.handle, 0);
}

static void VulkanSwapchainRecreate(Vulkan *vulkan, u32 windowWidth, u32 windowHeight) {
    // NOTE: get rid of this wait later for smooth swapchain resizing
    if (vulkan->vkDeviceWaitIdle(vulkan->logicalDevice) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkDeviceWaitIdle\n");
    };
    VulkanSwapchainDestroy(vulkan);
    VulkanSwapchainCreate(vulkan, windowWidth, windowHeight);
}

static bool VulkanFrameResourcesCreate(Vulkan *vulkan) {
    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vulkan->queueFamilies.graphicsIndex,
    };

    if (vulkan->vkCreateCommandPool(vulkan->logicalDevice, &commandPoolCreateInfo, 0, &vulkan->commandPool) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateCommandPool\n");

        return false;
    }

    VkPhysicalDeviceProperties properties;
    vulkan->vkGetPhysicalDeviceProperties(vulkan->physicalDevice, &properties);
    vulkan->uniformStride = (sizeof(u32) + properties.limits.minUniformBufferOffsetAlignment - 1) & ~(properties.limits.minUniformBufferOffsetAlignment - 1);

    for (u32 i = 0; i < FRAME_COUNT; i++) {
        VulkanFrameData *frameData = &vulkan->frameData[i];

        VkCommandBufferAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = vulkan->commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        if (vulkan->vkAllocateCommandBuffers(vulkan->logicalDevice, &allocateInfo, &frameData->commandBuffer) != VK_SUCCESS) {
            fprintf(stderr, "ERROR: vkAllocateCommandBuffers\n");

            return false;
        }

        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        if (vulkan->vkCreateSemaphore(vulkan->logicalDevice, &semaphoreCreateInfo, 0, &frameData->imageAvailableSemaphore) != VK_SUCCESS) {
            fprintf(stderr, "ERROR: vkCreateSemaphore\n");

            return false;
        }

        VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        if (vulkan->vkCreateFence(vulkan->logicalDevice, &fenceCreateInfo, 0, &frameData->inFlightFence) != VK_SUCCESS) {
            fprintf(stderr, "ERROR: vkCreateFence\n");

            return false;
        }

        frameData->uniformBuffer = VulkanBufferHostVisibleCreate(vulkan, vulkan->uniformStride * 4096, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, (void **)&frameData->uniformData);
        frameData->uniformCount = 0;
    }

    return true;
}

// TODO: This function does not return false yet but later it will.
static bool VulkanDescriptorSetsCreate(Vulkan *vulkan) {
    VkDescriptorPoolSize poolSizes[] = {
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = FRAME_COUNT},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = MAX_TEXTURES * FRAME_COUNT},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = FRAME_COUNT},
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = FRAME_COUNT * 2,
        .poolSizeCount = ArrayCount(poolSizes),
        .pPoolSizes = poolSizes,
    };

    if (vulkan->vkCreateDescriptorPool(vulkan->logicalDevice, &descriptorPoolCreateInfo, 0, &vulkan->descriptorPool) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateDescriptorPool\n");

        return false;
    };

    VkDescriptorSetLayoutBinding resourcesSetLayoutBinding[] = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = MAX_TEXTURES, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
    };

    VkDescriptorBindingFlags resourcesDescriptorBindingFlags[] = {0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo0 = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = ArrayCount(resourcesDescriptorBindingFlags),
        .pBindingFlags = resourcesDescriptorBindingFlags,
    };

    VkDescriptorSetLayoutCreateInfo resourcesDescriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flagsInfo0,
        .bindingCount = ArrayCount(resourcesSetLayoutBinding),
        .pBindings = resourcesSetLayoutBinding,
    };
    if (vulkan->vkCreateDescriptorSetLayout(vulkan->logicalDevice, &resourcesDescriptorSetLayoutCreateInfo, 0, &vulkan->resourceDescriptorLayout) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateDescriptorSetLayout\n");

        return false;
    };

    VkDescriptorSetLayoutBinding samplersSetLayoutBinding[] = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
    };

    VkDescriptorSetLayoutCreateInfo samplersDescriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ArrayCount(samplersSetLayoutBinding),
        .pBindings = samplersSetLayoutBinding,
    };
    if (vulkan->vkCreateDescriptorSetLayout(vulkan->logicalDevice, &samplersDescriptorSetLayoutCreateInfo, 0, &vulkan->samplerDescriptorSetLayout) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateDescriptorSetLayout\n");

        return false;
    };

    for (u32 i = 0; i < FRAME_COUNT; i++) {
        VkDescriptorSetLayout descriptorSetLayouts[] = {vulkan->resourceDescriptorLayout, vulkan->samplerDescriptorSetLayout};

        VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = vulkan->descriptorPool,
            .descriptorSetCount = 2,
            .pSetLayouts = descriptorSetLayouts,
        };

        VkDescriptorSet descriptorSets[2];
        if (vulkan->vkAllocateDescriptorSets(vulkan->logicalDevice, &descriptorSetAllocateInfo, descriptorSets) != VK_SUCCESS) {
            fprintf(stderr, "ERROR: vkAllocateDescriptorSets\n");

            return false;
        };
        vulkan->resourceDescriptorSets[i] = descriptorSets[0];
        vulkan->samplerDescriptorSets[i] = descriptorSets[1];

        VkDescriptorBufferInfo descriptorBufferCreateInfo = {.buffer = vulkan->frameData[i].uniformBuffer.handle, .offset = 0, .range = sizeof(u32)};
        VkWriteDescriptorSet writeResourcesDescriptorSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vulkan->resourceDescriptorSets[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .pBufferInfo = &descriptorBufferCreateInfo,
        };

        VkDescriptorImageInfo samplerDescriptorImageInfo = {.sampler = vulkan->textureSampler};
        VkWriteDescriptorSet writeSamplerDescriptorSet = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vulkan->samplerDescriptorSets[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &samplerDescriptorImageInfo,
        };

        VkWriteDescriptorSet descriptorSetWrites[] = {writeResourcesDescriptorSet, writeSamplerDescriptorSet};
        vulkan->vkUpdateDescriptorSets(vulkan->logicalDevice, 2, descriptorSetWrites, 0, 0);
    }

    return true;
}

static bool VulkanGraphicsPipelineCreate(Vulkan *vulkan) {
    VkShaderModuleCreateInfo vertexShaderCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = (const u32 *)g_VSMain,
        .codeSize = sizeof(g_VSMain),
    };

    VkShaderModuleCreateInfo fragmentShaderCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = (const u32 *)g_PSMain,
        .codeSize = sizeof(g_PSMain),
    };

    VkShaderModule vertexModule, fragmentModule;

    if (vulkan->vkCreateShaderModule(vulkan->logicalDevice, &vertexShaderCreateInfo, 0, &vertexModule) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateShaderModule\n");

        return false;
    }
    if (vulkan->vkCreateShaderModule(vulkan->logicalDevice, &fragmentShaderCreateInfo, 0, &fragmentModule) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateShaderModule\n");

        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexModule,
            .pName = "VSMain",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentModule,
            .pName = "PSMain",
        },
    };

    VkVertexInputBindingDescription bindingDescription = {
        .binding = 0,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride = sizeof(Vertex),
    };

    VkVertexInputAttributeDescription attributeDescription[] = {
        {
            .binding = 0,
            .location = 0,
            .format = VK_FORMAT_R32G32B32_SFLOAT,
            .offset = 0, // x, y, z
        },
        {
            .binding = 0,
            .location = 1,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .offset = sizeof(f32) * 3, // r, g, b, a
        },
        {
            .binding = 0,
            .location = 2,
            .format = VK_FORMAT_R32G32_SFLOAT,
            .offset = sizeof(f32) * 7, // u, v
        },
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &bindingDescription,
        .vertexAttributeDescriptionCount = ArrayCount(attributeDescription),
        .pVertexAttributeDescriptions = attributeDescription,
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineRasterizationStateCreateInfo rastezier = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .depthBiasEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment,
    };

    VkDynamicState dynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ArrayCount(dynamicStates),
        .pDynamicStates = dynamicStates,
    };

    VkPipelineRenderingCreateInfo renderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &vulkan->swapchain.format,
    };

    VkDescriptorSetLayout descriptorSetLayouts[] = {vulkan->resourceDescriptorLayout, vulkan->samplerDescriptorSetLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = ArrayCount(descriptorSetLayouts),
        .pSetLayouts = descriptorSetLayouts,
    };

    if (vulkan->vkCreatePipelineLayout(vulkan->logicalDevice, &pipelineLayoutCreateInfo, 0, &vulkan->pipelineLayout) != VK_SUCCESS) {
        // NOTE: fatal error so we do not care about destroying shader modules
        fprintf(stderr, "ERROR: vkCreatePipelineLayout\n");

        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCreateInfo,
        .stageCount = ArrayCount(shaderStages),
        .pStages = shaderStages,
        .pVertexInputState = &vertexInputInfo,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rastezier,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .pDynamicState = &dynamicState,
        .layout = vulkan->pipelineLayout,
        .renderPass = VK_NULL_HANDLE,
    };

    if (vulkan->vkCreateGraphicsPipelines(vulkan->logicalDevice, VK_NULL_HANDLE, 1, &pipelineCreateInfo, 0, &vulkan->pipeline) != VK_SUCCESS) {
        // NOTE: fatal error so we do not care about destroying shader modules
        fprintf(stderr, "ERROR: vkCreateGraphicsPipelines\n");

        return false;
    }

    vulkan->vkDestroyShaderModule(vulkan->logicalDevice, vertexModule, 0);
    vulkan->vkDestroyShaderModule(vulkan->logicalDevice, fragmentModule, 0);

    return true;
}

static bool VulkanSamplerCreate(Vulkan *vulkan) {
    VkSamplerCreateInfo samplerCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .maxAnisotropy = 1.0f,
    };

    if (vulkan->vkCreateSampler(vulkan->logicalDevice, &samplerCreateInfo, 0, &vulkan->textureSampler) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateSampler\n");

        return false;
    }

    return true;
}

u32 VulkanTextureCreate(Vulkan *vulkan, u32 index, Vector2U size, u32 bytesPerPixel, const void *pixels) {
    if (vulkan->textureCount + 1 > MAX_TEXTURES) {
        fprintf(stderr, "ERROR: maximum number of textures reached: %u.\n", vulkan->textureCount);
    }

    VulkanImage result = {0};

    VkImageCreateInfo imageCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent = {size.width, size.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
    };

    if (vulkan->vkCreateImage(vulkan->logicalDevice, &imageCreateInfo, 0, &result.handle) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateImage\n");

        return 0;
    }

    VkMemoryRequirements memoryRequirements;
    vulkan->vkGetImageMemoryRequirements(vulkan->logicalDevice, result.handle, &memoryRequirements);

    VkMemoryAllocateInfo allocationInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = VulkanMemoryTypeFind(vulkan, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    if (vulkan->vkAllocateMemory(vulkan->logicalDevice, &allocationInfo, 0, &result.memory) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkAllocateMemory\n");

        return 0;
    }

    if (vulkan->vkBindImageMemory(vulkan->logicalDevice, result.handle, result.memory, 0) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkBindImageMemory\n");

        return 0;
    }

    VkImageViewCreateInfo imageViewCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = result.handle,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_SRGB,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
            .levelCount = 1,
        },
    };

    if (vulkan->vkCreateImageView(vulkan->logicalDevice, &imageViewCreateInfo, 0, &result.view) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateImageView\n");

        return 0;
    }

    u8 *mappedData;
    VulkanBuffer stagingBuffer = VulkanBufferHostVisibleCreate(vulkan, size.width * size.height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, (void **)&mappedData);

    const u8 *sourcePixels = (const u8 *)pixels;
    for (u32 y = 0; y < size.height; ++y) {
        u8 *destinationRow = mappedData + (y * size.width * 4);
        const u8 *sourceRow = sourcePixels + (y * size.width * bytesPerPixel);

        if (bytesPerPixel == 4) {
            MemoryCopyForwards(destinationRow, sourceRow, size.width * 4);
        } else if (bytesPerPixel == 3) {
            for (u32 x = 0; x < size.width; ++x) {
                destinationRow[x * 4 + 0] = sourceRow[x * 3 + 0];
                destinationRow[x * 4 + 1] = sourceRow[x * 3 + 1];
                destinationRow[x * 4 + 2] = sourceRow[x * 3 + 2];
                destinationRow[x * 4 + 3] = 255;
            }
        }
    }

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = vulkan->queueFamilies.graphicsIndex,
    };

    if (vulkan->vkCreateCommandPool(vulkan->logicalDevice, &commandPoolCreateInfo, 0, &commandPool) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkCreateCommandPool\n");

        return 0;
    }

    VkCommandBufferAllocateInfo commandBufferAllocationInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = 1,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    if (vulkan->vkAllocateCommandBuffers(vulkan->logicalDevice, &commandBufferAllocationInfo, &commandBuffer) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkAllocateCommandBuffers\n");

        return 0;
    }

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vulkan->vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkBeginCommandBuffer\n");

        return 0;
    }

    VkImageMemoryBarrier imageMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = result.handle,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
            .levelCount = 1,
        },
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    };

    vulkan->vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &imageMemoryBarrier);

    VkBufferImageCopy copyRegion = {
        .imageExtent = (VkExtent3D){size.width, size.height, 1},
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
    };

    vulkan->vkCmdCopyBufferToImage(commandBuffer, stagingBuffer.handle, result.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    imageMemoryBarrier = (VkImageMemoryBarrier){
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = result.handle,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
            .levelCount = 1,
        },
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
    };

    vulkan->vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &imageMemoryBarrier);

    if (vulkan->vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkEndCommandBuffer\n");

        return 0;
    }

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    if (vulkan->vkQueueSubmit(vulkan->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkQueueSubmit\n");

        return 0;
    }

    if (vulkan->vkQueueWaitIdle(vulkan->graphicsQueue) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkQueueWaitIdle\n");

        return 0;
    }

    vulkan->vkDestroyCommandPool(vulkan->logicalDevice, commandPool, 0);
    VulkanBufferHostVisibleDestroy(vulkan, stagingBuffer);

    vulkan->textures[index] = result;
    vulkan->textureCount++;

    VkDescriptorImageInfo imageInfo = {
        .sampler = vulkan->textureSampler,
        .imageView = result.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet descriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .dstBinding = 1,
        .descriptorCount = 1,
        .pImageInfo = &imageInfo,
        .dstArrayElement = index,
    };

    for (u32 i = 0; i < FRAME_COUNT; i++) {
        descriptorWrite.dstSet = vulkan->resourceDescriptorSets[i];
        vulkan->vkUpdateDescriptorSets(vulkan->logicalDevice, 1, &descriptorWrite, 0, 0);
    }

    return index;
}

static bool VulkanCommonInitialize(Vulkan *vulkan, LinuxWayland *window) {
#define INSTANCE_FUNCTION(name) \
    vulkan->name = (PFN_##name)vulkan->vkGetInstanceProcAddr(vulkan->instance, #name);
    VULKAN_INSTANCE_FUNCTIONS
#undef INSTANCE_FUNCTION

#if defined(DEBUG)
#define DEBUG_FUNCTION(name) \
    vulkan->name = (PFN_##name)vulkan->vkGetInstanceProcAddr(vulkan->instance, #name);
    VULKAN_DEBUG_FUNCTIONS
#undef DEBUG_FUNCTION
    VulkanDebugMessengerCreate(vulkan);
#endif

    if (!VulkanPhysicalDevicePick(vulkan)) {
        return false;
    }
    if (!VulkanLogicalDeviceCreate(vulkan)) {
        return false;
    }

#define DEVICE_FUNCTION(name) \
    vulkan->name = (PFN_##name)vulkan->vkGetDeviceProcAddr(vulkan->logicalDevice, #name);
    VULKAN_DEVICE_FUNCTIONS
#undef DEVICE_FUNCTION

    vulkan->vkGetDeviceQueue(vulkan->logicalDevice, vulkan->queueFamilies.graphicsIndex, 0, &vulkan->graphicsQueue);
    vulkan->vkGetDeviceQueue(vulkan->logicalDevice, vulkan->queueFamilies.presentIndex, 0, &vulkan->presentQueue);

    if (!VulkanSwapchainCreate(vulkan, window->width, window->height)) {
        return false;
    }
    if (!VulkanFrameResourcesCreate(vulkan)) {
        return false;
    }
    if (!VulkanSamplerCreate(vulkan)) {
        return false;
    }
    if (!VulkanDescriptorSetsCreate(vulkan)) {
        return false;
    }
    if (!VulkanGraphicsPipelineCreate(vulkan)) {
        return false;
    }

    u32 whitePixel = 0xFFFFFFFF;
    VulkanTextureCreate(vulkan, 0, (Vector2U){1, 1}, 4, &whitePixel);

    vulkan->vertexCount = 0;
    vulkan->vertexCapacity = 4096;
    vulkan->vertexBuffer = VulkanBufferHostVisibleCreate(vulkan, vulkan->vertexCapacity, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, (void **)&vulkan->vertexData);

    vulkan->frameIndex = 0;
    vulkan->textureCount = 0;

    return true;
}

// NOTE: divide this into windows and linux versions for the future
bool VulkanInitialize(Vulkan *vulkan, LinuxWayland *window) {
    MemoryZero(vulkan, sizeof(Vulkan));

    void *libVulkan = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!libVulkan) {
        fprintf(stderr, "%s\n", dlerror());

        return false;
    }

    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(libVulkan, "vkGetInstanceProcAddr");
    PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(0, "vkCreateInstance");

    VulkanInstanceCreate(vulkan, vkCreateInstance, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
    vulkan->vkGetInstanceProcAddr = vkGetInstanceProcAddr;

    PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR = (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(vulkan->instance, "vkCreateWaylandSurfaceKHR");

    VkWaylandSurfaceCreateInfoKHR surfaceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = window->display,
        .surface = window->surface,
    };

    if (vkCreateWaylandSurfaceKHR(vulkan->instance, &surfaceCreateInfo, 0, &vulkan->surface) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: Failed to create wayland vulkan surface.\n");

        return false;
    }

    VulkanCommonInitialize(vulkan, window);

    return true;
}

static void VulkanFramePassTransfer(Vulkan *vulkan, const RenderCommandBuffer *commandBuffer) {
    usize memoryOffset = 0;

    while (memoryOffset < commandBuffer->currentOffset) {
        RenderCommandHeader *header = (RenderCommandHeader *)(commandBuffer->basePointer + memoryOffset);

        if (header->size == 0 || memoryOffset + header->size > commandBuffer->currentOffset) {
            break;
        }

        if (header->type == RenderCommandType_AllocateTexture) {
            RenderCommandAllocateTexture *command = (RenderCommandAllocateTexture *)header;
            VulkanTextureCreate(vulkan, command->index, command->size, command->bytesPerPixel, command->pixels);
        }

        memoryOffset += header->size;
    }
}

static void VulkanFramePassRender(Vulkan *vulkan, const RenderCommandBuffer *commandBuffer) {
    VulkanFrameData *frameData = &vulkan->frameData[vulkan->frameIndex];
    usize memoryOffset = 0;

    while (memoryOffset < commandBuffer->currentOffset) {
        RenderCommandHeader *header = (RenderCommandHeader *)(commandBuffer->basePointer + memoryOffset);
        if (header->size == 0 || memoryOffset + header->size > commandBuffer->currentOffset) {
            break;
        }

        switch (header->type) {
        case RenderCommandType_ClearEntireScreen: {
            RenderCommandClearEntireScreen *command = (RenderCommandClearEntireScreen *)header;

            vulkan->clearColor = (VkClearValue){
                .color = {{
                    command->color.r,
                    command->color.g,
                    command->color.b,
                    command->color.a,
                }},
            };

        } break;

        case RenderCommandType_DrawRectangle: {
            RenderCommandDrawRectangle *command = (RenderCommandDrawRectangle *)header;

            const u32 verticesPerRectangle = 6;
            if (vulkan->vertexCount + verticesPerRectangle <= vulkan->vertexCapacity) {
                f32 leftEdge = (command->position.x / (f32)DEFAULT_WINDOW_WIDTH) * 2.0f - 1.0f;
                f32 rightEdge = ((command->position.x + command->size.x) / (f32)DEFAULT_WINDOW_WIDTH) * 2.0f - 1.0f;
                f32 topEdge = 1.0f - (command->position.y / (f32)DEFAULT_WINDOW_HEIGHT) * 2.0f;
                f32 bottomEdge = 1.0f - ((command->position.y + command->size.y) / (f32)DEFAULT_WINDOW_HEIGHT) * 2.0f;

                Vertex topLeft = {{leftEdge, topEdge, 0.0f}, {command->color.r, command->color.g, command->color.b, command->color.a}, {0.0f, 0.0f}};
                Vertex topRight = {{rightEdge, topEdge, 0.0f}, {command->color.r, command->color.g, command->color.b, command->color.a}, {1.0f, 0.0f}};
                Vertex bottomLeft = {{leftEdge, bottomEdge, 0.0f}, {command->color.r, command->color.g, command->color.b, command->color.a}, {0.0f, 1.0f}};
                Vertex bottomRight = {{rightEdge, bottomEdge, 0.0f}, {command->color.r, command->color.g, command->color.b, command->color.a}, {1.0f, 1.0f}};

                Vertex *currentVertexDestination = (Vertex *)vulkan->vertexData + vulkan->vertexCount;
                currentVertexDestination[0] = topLeft;
                currentVertexDestination[1] = topRight;
                currentVertexDestination[2] = bottomLeft;
                currentVertexDestination[3] = bottomLeft;
                currentVertexDestination[4] = topRight;
                currentVertexDestination[5] = bottomRight;

                VkDeviceSize offset = vulkan->vertexCount * sizeof(Vertex);
                vulkan->vkCmdBindVertexBuffers(frameData->commandBuffer, 0, 1, &vulkan->vertexBuffer.handle, &offset);

                u32 *destination = (u32 *)(frameData->uniformData + (frameData->uniformCount * vulkan->uniformStride));

                *destination = command->texture;

                u32 dynamicOffset = frameData->uniformCount * vulkan->uniformStride;
                vulkan->vkCmdBindDescriptorSets(frameData->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->pipelineLayout, 0, 1, &vulkan->resourceDescriptorSets[vulkan->frameIndex], 1, &dynamicOffset);

                vulkan->vkCmdDraw(frameData->commandBuffer, 6, 1, 0, 0);
            }
        } break;
        }

        memoryOffset += header->size;
    }
}

bool VulkanFrameBegin(Vulkan *vulkan, LinuxWayland *window, RenderCommandBuffer *commandBuffer) {
    VulkanFrameData *frameData = &vulkan->frameData[vulkan->frameIndex];

    if (commandBuffer && commandBuffer->basePointer && !commandBuffer->hasOverflowed) {
        VulkanFramePassTransfer(vulkan, commandBuffer);
    }

    if (vulkan->swapchain.extent.width != window->width || vulkan->swapchain.extent.height != window->height) {
        VulkanSwapchainRecreate(vulkan, window->width, window->height);
        return false;
    }

    if (vulkan->vkWaitForFences(vulkan->logicalDevice, 1, &frameData->inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkWaitForFences\n");

        return false;
    };
    VkResult result = vulkan->vkAcquireNextImageKHR(vulkan->logicalDevice, vulkan->swapchain.handle, UINT64_MAX, frameData->imageAvailableSemaphore, 0, &vulkan->imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        VulkanSwapchainRecreate(vulkan, window->width, window->height);
        return false;
    }

    if (vulkan->vkResetFences(vulkan->logicalDevice, 1, &frameData->inFlightFence) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkResetFences\n");

        return false;
    };
    if (vulkan->vkResetCommandBuffer(frameData->commandBuffer, 0) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkResetCommandBuffer\n");

        return false;
    };

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vulkan->vkBeginCommandBuffer(frameData->commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkBeginCommandBuffer\n");
    }

    VkImageMemoryBarrier imageMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vulkan->swapchain.images[vulkan->imageIndex],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
            .levelCount = 1,
        },
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    vulkan->vkCmdPipelineBarrier(frameData->commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, 0, 0, 0, 1, &imageMemoryBarrier);

    VkRenderingAttachmentInfo colorAttachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = vulkan->swapchain.imageViews[vulkan->imageIndex],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = vulkan->clearColor,
    };

    VkRenderingInfo renderingInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorAttachmentInfo,
        .renderArea = {
            .offset = (VkOffset2D){0, 0},
            .extent = vulkan->swapchain.extent,
        },
    };

    vulkan->vkCmdBeginRendering(frameData->commandBuffer, &renderingInfo);

    VkViewport viewport = {
        .x = 0.0f,
        .y = (float)vulkan->swapchain.extent.height,
        .width = (float)vulkan->swapchain.extent.width,
        .height = -(float)vulkan->swapchain.extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .offset = {0, 0},
        .extent = vulkan->swapchain.extent,
    };

    vulkan->vkCmdSetViewport(frameData->commandBuffer, 0, 1, &viewport);
    vulkan->vkCmdSetScissor(frameData->commandBuffer, 0, 1, &scissor);

    vulkan->vkCmdBindPipeline(frameData->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->pipeline);
    vulkan->vkCmdBindDescriptorSets(frameData->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->pipelineLayout, 1, 1, &vulkan->samplerDescriptorSets[vulkan->frameIndex], 0, 0);

    frameData->uniformCount = 0;

    return true;
}

bool VulkanFrameEnd(Vulkan *vulkan, RenderCommandBuffer *commandBuffer) {
    VulkanFrameData *frameData = &vulkan->frameData[vulkan->frameIndex];

    if (commandBuffer && commandBuffer->basePointer && !commandBuffer->hasOverflowed) {
        VulkanFramePassRender(vulkan, commandBuffer);
    }

    vulkan->vkCmdEndRendering(frameData->commandBuffer);

    VkImageMemoryBarrier imageMemoryBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vulkan->swapchain.images[vulkan->imageIndex],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
            .levelCount = 1,
        },
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    vulkan->vkCmdPipelineBarrier(frameData->commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, 0, 0, 0, 1, &imageMemoryBarrier);

    if (vulkan->vkEndCommandBuffer(frameData->commandBuffer) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkEndCommandBuffer\n");

        return false;
    }

    VkSemaphore waitSemaphores[] = {
        frameData->imageAvailableSemaphore,
    };

    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    VkSemaphore signalSemaphores[] = {
        vulkan->swapchain.renderFinishedSemaphore[vulkan->imageIndex],
    };

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &frameData->commandBuffer,
        .waitSemaphoreCount = ArrayCount(waitSemaphores),
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .signalSemaphoreCount = ArrayCount(signalSemaphores),
        .pSignalSemaphores = signalSemaphores,
    };

    if (vulkan->vkQueueSubmit(vulkan->graphicsQueue, 1, &submitInfo, frameData->inFlightFence) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkQueueSubmit\n");

        return false;
    }

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &vulkan->swapchain.handle,
        .pImageIndices = &vulkan->imageIndex,
        .waitSemaphoreCount = ArrayCount(signalSemaphores),
        .pWaitSemaphores = signalSemaphores,
    };

    if (vulkan->vkQueuePresentKHR(vulkan->presentQueue, &presentInfo) != VK_SUCCESS) {
        fprintf(stderr, "ERROR: vkQueuePresentKHR\n");

        return false;
    }

    vulkan->vertexCount = 0;
    vulkan->frameIndex = (vulkan->frameIndex + 1) % FRAME_COUNT;

    return true;
}
