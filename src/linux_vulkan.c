
#include "linux_vulkan.h"
#include "game_platform.h"
#include "game_types.h"
#include "linux.h"

#include <stdio.h>
#include <dlfcn.h>
#include <assert.h>
#include <vulkan/vulkan_core.h>

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;

    u32 formatCount;
    VkSurfaceFormatKHR formats[32];

    u32 presentModeCount;
    VkPresentModeKHR presentModes[8];
} VulkanSwapchainSupport;

typedef struct {
    u32 textureIndex;
} VulkanPushConstant;

static u8 vertexShaderData[] = {
#include "BasicGeometry.vert.spv.h"
};

static u8 fragmentShaderData[] = {
#include "BasicGeometry.frag.spv.h"
};

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
        printf("ERROR: failed to create vulkan host visible buffer.\n");

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
        printf("ERROR: failed to allocate vulkan memory.\n");

        return result;
    }

    if (vulkan->vkBindBufferMemory(vulkan->logicalDevice, result.handle, result.memory, 0) != VK_SUCCESS) {
        printf("ERROR: failed to bind vulkan host visible buffer.\n");

        return result;
    }

    if (vulkan->vkMapMemory(vulkan->logicalDevice, result.memory, 0, result.size, 0, mapped) != VK_SUCCESS) {
        printf("ERROR: failed to map vulkan host visible memroy.\n");

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
    printf("Validation layers will be enabled\n");

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
        .enabledExtensionCount = Array_Count(extensions),
        .ppEnabledExtensionNames = extensions,
#if defined(DEBUG)
        .enabledLayerCount = Array_Count(layers),
        .ppEnabledLayerNames = layers,
#endif
    };

    if (vkCreateInstance(&instanceCreateInfo, 0, &vulkan->instance) != VK_SUCCESS) {
        printf("ERROR: failed to create vulkan instance.\n");

        return;
    }
}

#if defined(DEBUG)

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanDebugMessengerCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *userData) {
    Unused(severity);
    Unused(type);
    Unused(userData);
    printf("%s\n", data->pMessage);

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
        printf("ERROR: failed to create vulkan debug messenger.\n");

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

static void VulkanPhysicalDevicePick(Vulkan *vulkan) {
    u32 count = 0;
    vulkan->vkEnumeratePhysicalDevices(vulkan->instance, &count, 0);

    count = Min(count, 8);

    VkPhysicalDevice devices[8];
    vulkan->vkEnumeratePhysicalDevices(vulkan->instance, &count, devices);

    for (u32 i = 0; i < count; i++) {
        if (VulkanPhysicalDeviceSuitable(vulkan, devices[i])) {
            vulkan->physicalDevice = devices[i];

            break;
        }
    }

    if (!vulkan->physicalDevice) {
        printf("WARNING: I did not find a dedicated GPU!\n");
        vulkan->physicalDevice = devices[0];
    }

    if (!vulkan->physicalDevice) {
        printf("ERROR: Your GPU is not supported by vulkan!\n");
    }

    VkPhysicalDeviceProperties properties;
    vulkan->vkGetPhysicalDeviceProperties(vulkan->physicalDevice, &properties);
    printf("GPU: %s\n", properties.deviceName);
}

static void VulkanLogicalDeviceCreate(Vulkan *vulkan) {
    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    f32 queuePriority = 1.0f;

    u32 uniqueIndices[2];
    u32 uniqueCount = 0;

    uniqueIndices[uniqueCount++] = vulkan->queueFamilies.graphicsIndex;
    if (vulkan->queueFamilies.presentIndex != vulkan->queueFamilies.graphicsIndex) {
        printf("Graphics and present queue are from diffrent families.\n");

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

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES,
        .pNext = &dynamicRendering,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    };

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &indexingFeatures,
        .pQueueCreateInfos = queueCreateInfos,
        .queueCreateInfoCount = uniqueCount,
        .enabledExtensionCount = Array_Count(extensions),
        .ppEnabledExtensionNames = extensions,
    };

    if (vulkan->vkCreateDevice(vulkan->physicalDevice, &deviceCreateInfo, 0, &vulkan->logicalDevice) != VK_SUCCESS) {
        printf("ERROR: failed to create vulkan device.\n");

        return;
    }
}

static VulkanSwapchainSupport VulkanSwapchainSupportQuery(Vulkan *vulkan) {
    VulkanSwapchainSupport result = {0};

    vulkan->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan->physicalDevice, vulkan->surface, &result.capabilities);
    vulkan->vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physicalDevice, vulkan->surface, &result.formatCount, 0);
    if (!result.formatCount) {
        printf("ERROR: failed to get vulkan surface formats.\n");

        return result;
    }

    // NOTE: There will be a segmentation fault if we don't clamp this because drivers can sometimes return just a lot of them.
    result.formatCount = Min(result.formatCount, 32);
    vulkan->vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physicalDevice, vulkan->surface, &result.formatCount, result.formats);

    vulkan->vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->physicalDevice, vulkan->surface, &result.presentModeCount, 0);
    if (!result.presentModeCount) {
        printf("ERROR: failed to get vulkan surface present modes.\n");

        return result;
    }
    result.presentModeCount = Min(result.presentModeCount, 8);
    vulkan->vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->physicalDevice, vulkan->surface, &result.presentModeCount, result.presentModes);

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

static void VulkanSwapchainCreate(Vulkan *vulkan, u32 windowWidth, u32 windowHeight) {
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
        printf("ERROR: failed to create vulkan swapchain.\n");

        return;
    }
    swapchain->format = surfaceFormat.format;
    swapchain->extent = extent;

    vulkan->vkGetSwapchainImagesKHR(vulkan->logicalDevice, swapchain->handle, &swapchain->imageCount, 0);
    assert(swapchain->imageCount <= VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT);
    vulkan->vkGetSwapchainImagesKHR(vulkan->logicalDevice, swapchain->handle, &swapchain->imageCount, swapchain->images);

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
            printf("ERROR: failed to create vulkan swapchain image view.\n");

            return;
        }

        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        if (vulkan->vkCreateSemaphore(vulkan->logicalDevice, &semaphoreCreateInfo, 0, &swapchain->renderFinishedSemaphore[i]) != VK_SUCCESS) {
            printf("ERROR: failed to create vulkan semaphore.\n");

            return;
        }
    }
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
    vulkan->vkDeviceWaitIdle(vulkan->logicalDevice);
    VulkanSwapchainDestroy(vulkan);
    VulkanSwapchainCreate(vulkan, windowWidth, windowHeight);
}

static void VulkanFrameResourcesCreate(Vulkan *vulkan) {
    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vulkan->queueFamilies.graphicsIndex,
    };

    if (vulkan->vkCreateCommandPool(vulkan->logicalDevice, &commandPoolCreateInfo, 0, &vulkan->commandPool) != VK_SUCCESS) {
        printf("ERROR: failed to create vulkan command pool.\n");

        return;
    }

    for (u32 i = 0; i < FRAME_COUNT; i++) {
        VulkanFrameData *frameData = &vulkan->frameData[i];

        VkCommandBufferAllocateInfo allocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = vulkan->commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        if (vulkan->vkAllocateCommandBuffers(vulkan->logicalDevice, &allocateInfo, &frameData->commandBuffer) != VK_SUCCESS) {
            printf("ERROR: failed to create vulkan command buffer.\n");

            return;
        }

        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        if (vulkan->vkCreateSemaphore(vulkan->logicalDevice, &semaphoreCreateInfo, 0, &frameData->imageAvailableSemaphore) != VK_SUCCESS) {
            printf("ERROR: failed to create vulkan semaphore.\n");

            return;
        }

        VkFenceCreateInfo fenceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        if (vulkan->vkCreateFence(vulkan->logicalDevice, &fenceCreateInfo, 0, &frameData->inFlightFence) != VK_SUCCESS) {
            printf("ERROR: failed to create vulkan fence.\n");

            return;
        }
    }
}

static void VulkanDescriptorSetsCreate(Vulkan *vulkan) {
    VkDescriptorPoolSize poolSizes[] = {
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_TEXTURES * FRAME_COUNT,
        },
    };

    VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = FRAME_COUNT,
        .poolSizeCount = Array_Count(poolSizes),
        .pPoolSizes = poolSizes,
    };

    if (vulkan->vkCreateDescriptorPool(vulkan->logicalDevice, &descriptorPoolCreateInfo, 0, &vulkan->descriptorPool) != VK_SUCCESS) {
        printf("ERROR: failed to create vulkan descriptor pool.\n");

        return;
    }

    VkDescriptorSetLayoutBinding descriptorBindings[] = {
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = MAX_TEXTURES,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
        },
    };

    VkDescriptorBindingFlags descriptorBindingFlags[] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT,
    };

    VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetBindingFlagsInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = Array_Count(descriptorBindings),
        .pBindingFlags = descriptorBindingFlags,
    };

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &descriptorSetBindingFlagsInfo,
        .bindingCount = Array_Count(descriptorBindings),
        .pBindings = descriptorBindings,
    };

    if (vulkan->vkCreateDescriptorSetLayout(vulkan->logicalDevice, &descriptorSetLayoutCreateInfo, 0, &vulkan->descriptorLayout) != VK_SUCCESS) {
        printf("ERROR: failed to create vulkan descriptor set layout.\n");

        return;
    }

    VkDescriptorSetLayout perSetLayouts[] = {
        vulkan->descriptorLayout,
        vulkan->descriptorLayout,
        vulkan->descriptorLayout,
    };

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorSetCount = FRAME_COUNT,
        .pSetLayouts = perSetLayouts,
        .descriptorPool = vulkan->descriptorPool,
    };

    if (vulkan->vkAllocateDescriptorSets(vulkan->logicalDevice, &descriptorSetAllocateInfo, vulkan->descriptorSets) != VK_SUCCESS) {
        printf("ERROR: failed to allocate vulkan descriptor sets.\n");

        return;
    }
}

static void VulkanGraphicsPipelineCreate(Vulkan *vulkan) {
    VkShaderModuleCreateInfo vertexShaderCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(vertexShaderData),
        .pCode = (const u32 *)vertexShaderData,
    };

    VkShaderModuleCreateInfo fragmentShaderCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = sizeof(fragmentShaderData),
        .pCode = (const u32 *)fragmentShaderData,
    };

    VkShaderModule vertexModule, fragmentModule;

    if (vulkan->vkCreateShaderModule(vulkan->logicalDevice, &vertexShaderCreateInfo, 0, &vertexModule) != VK_SUCCESS) {
        printf("ERROR: failed to create vulkan vertex shader module.\n");

        return;
    }
    if (vulkan->vkCreateShaderModule(vulkan->logicalDevice, &fragmentShaderCreateInfo, 0, &fragmentModule) != VK_SUCCESS) {
        printf("ERROR: failed to create vulkan fragment shader module.\n");

        return;
    }

    VkPipelineShaderStageCreateInfo shaderStages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexModule,
            .pName = "main",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentModule,
            .pName = "main",
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
        .vertexAttributeDescriptionCount = Array_Count(attributeDescription),
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
        .dynamicStateCount = Array_Count(dynamicStates),
        .pDynamicStates = dynamicStates,
    };

    VkPipelineRenderingCreateInfo renderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &vulkan->swapchain.format,
    };

    VkPushConstantRange pushConstantRange = {
        .size = sizeof(VulkanPushConstant),
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
    };

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pPushConstantRanges = &pushConstantRange,
        .pushConstantRangeCount = 1,
        .setLayoutCount = 1,
        .pSetLayouts = &vulkan->descriptorLayout,
    };

    if (vulkan->vkCreatePipelineLayout(vulkan->logicalDevice, &pipelineLayoutCreateInfo, 0, &vulkan->pipelineLayout) != VK_SUCCESS) {
        // NOTE: fatal error so we do not care about destroying shader modules
        printf("ERROR: failed to create vulkan pipeline layout.\n");

        return;
    }

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderingCreateInfo,
        .stageCount = Array_Count(shaderStages),
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
        printf("ERROR: failed to create vulkan pipeline.\n");

        return;
    }

    vulkan->vkDestroyShaderModule(vulkan->logicalDevice, vertexModule, 0);
    vulkan->vkDestroyShaderModule(vulkan->logicalDevice, fragmentModule, 0);
}

static void VulkanSamplerCreate(Vulkan *vulkan) {
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
        printf("ERROR: failed to create vulkan sampler.\n");

        return;
    }
}

static void VulkanCommonInitialize(Vulkan *vulkan, LinuxWayland *window) {
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

    VulkanPhysicalDevicePick(vulkan);
    VulkanLogicalDeviceCreate(vulkan);

#define DEVICE_FUNCTION(name) \
    vulkan->name = (PFN_##name)vulkan->vkGetDeviceProcAddr(vulkan->logicalDevice, #name);
    VULKAN_DEVICE_FUNCTIONS
#undef DEVICE_FUNCTION

    vulkan->vkGetDeviceQueue(vulkan->logicalDevice, vulkan->queueFamilies.graphicsIndex, 0, &vulkan->graphicsQueue);
    vulkan->vkGetDeviceQueue(vulkan->logicalDevice, vulkan->queueFamilies.presentIndex, 0, &vulkan->presentQueue);

    VulkanSwapchainCreate(vulkan, window->width, window->height);
    VulkanFrameResourcesCreate(vulkan);
    VulkanDescriptorSetsCreate(vulkan);
    VulkanGraphicsPipelineCreate(vulkan);
    VulkanSamplerCreate(vulkan);

    vulkan->vertexCount = 0;
    vulkan->vertexCapacity = 4096;
    vulkan->vertexBuffer = VulkanBufferHostVisibleCreate(vulkan, vulkan->vertexCapacity, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, (void **)&vulkan->vertexData);

    vulkan->frameIndex = 0;
    vulkan->textureCount = 0;
}

// NOTE: divide this into windows and linux versions for the future
void VulkanInitialize(Vulkan *vulkan, LinuxWayland *window) {
    MemoryZero(vulkan, sizeof(Vulkan));

    void *libVulkan = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!libVulkan) {
        printf("ERROR: Could not load libvulkan.so.1: %s\n", dlerror());
        return;
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
        printf("ERROR: failed to create wayland vulkan surface.\n");

        return;
    }

    VulkanCommonInitialize(vulkan, window);
}

u32 VulkanTextureCreate(Vulkan *vulkan, u32 index, Vector2U size, u32 bytesPerPixel, const void *pixels) {
    if (vulkan->textureCount + 1 > MAX_TEXTURES) {
        printf("ERROR: maximum number of textures reached: %u.\n", vulkan->textureCount);
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
        printf("ERROR: failed to create vulkan image.\n");

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
        printf("ERROR: failed to allocate vulkan image memory.\n");

        return 0;
    }

    if (vulkan->vkBindImageMemory(vulkan->logicalDevice, result.handle, result.memory, 0) != VK_SUCCESS) {
        printf("ERROR: failed to bind vulkan image memory.\n");

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
        printf("ERROR: failed to create vulkan image view.\n");

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
        printf("ERROR: failed to create vulkan staging command pool.\n");

        return 0;
    }

    VkCommandBufferAllocateInfo commandBufferAllocationInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = 1,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    if (vulkan->vkAllocateCommandBuffers(vulkan->logicalDevice, &commandBufferAllocationInfo, &commandBuffer) != VK_SUCCESS) {
        printf("ERROR: failed to allocate vulkan staging command buffer.\n");

        return 0;
    }

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vulkan->vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
        printf("ERROR: failed to beging recording into vulkan staging command buffer.\n");

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
        printf("ERROR: failed to end recording into vulkan staging command buffer.\n");

        return 0;
    }

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    if (vulkan->vkQueueSubmit(vulkan->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        printf("ERROR: failed to submit vulkan staging buffer command.\n");

        return 0;
    }

    if (vulkan->vkQueueWaitIdle(vulkan->graphicsQueue) != VK_SUCCESS) {
        printf("ERROR: failed to wait for vulkan graphics queue.\n");

        return 0;
    }

    vulkan->vkDestroyCommandPool(vulkan->logicalDevice, commandPool, 0);
    VulkanBufferHostVisibleDestroy(vulkan, stagingBuffer);

    vulkan->textures[index - 1] = result;
    vulkan->textureCount++;

    VkDescriptorImageInfo imageInfo = {
        .sampler = vulkan->textureSampler,
        .imageView = result.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet descriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .dstBinding = 0,
        .descriptorCount = 1,
        .pImageInfo = &imageInfo,
        .dstArrayElement = index - 1,
    };

    for (u32 i = 0; i < FRAME_COUNT; i++) {
        descriptorWrite.dstSet = vulkan->descriptorSets[i];
        vulkan->vkUpdateDescriptorSets(vulkan->logicalDevice, 1, &descriptorWrite, 0, 0);
    }

    return index;
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

                VulkanPushConstant pushConstant = {
                    .textureIndex = command->texture,
                };

                vulkan->vkCmdPushConstants(frameData->commandBuffer, vulkan->pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(VulkanPushConstant), &pushConstant);

                vulkan->vkCmdDraw(frameData->commandBuffer, 6, 1, 0, 0);

                vulkan->vertexCount += verticesPerRectangle;
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

    vulkan->vkWaitForFences(vulkan->logicalDevice, 1, &frameData->inFlightFence, VK_TRUE, UINT64_MAX);
    VkResult result = vulkan->vkAcquireNextImageKHR(vulkan->logicalDevice, vulkan->swapchain.handle, UINT64_MAX, frameData->imageAvailableSemaphore, 0, &vulkan->imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        VulkanSwapchainRecreate(vulkan, window->width, window->height);
        return false;
    }

    vulkan->vkResetFences(vulkan->logicalDevice, 1, &frameData->inFlightFence);
    vulkan->vkResetCommandBuffer(frameData->commandBuffer, 0);

    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    if (vulkan->vkBeginCommandBuffer(frameData->commandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
        printf("ERROR: could not beging command buffer.\n");
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
        .y = 0.0f,
        .width = (float)vulkan->swapchain.extent.width,
        .height = (float)vulkan->swapchain.extent.height,
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
    vulkan->vkCmdBindDescriptorSets(frameData->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->pipelineLayout, 0, 1, &vulkan->descriptorSets[vulkan->frameIndex], 0, 0);

    return true;
}

void VulkanFrameEnd(Vulkan *vulkan, RenderCommandBuffer *commandBuffer) {
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
        printf("ERROR: failed to end recording vulkan command buffer.\n");

        return;
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
        .waitSemaphoreCount = Array_Count(waitSemaphores),
        .pWaitSemaphores = waitSemaphores,
        .pWaitDstStageMask = waitStages,
        .signalSemaphoreCount = Array_Count(signalSemaphores),
        .pSignalSemaphores = signalSemaphores,
    };

    if (vulkan->vkQueueSubmit(vulkan->graphicsQueue, 1, &submitInfo, frameData->inFlightFence) != VK_SUCCESS) {
        printf("ERROR: failed to present graphics queue.\n");

        return;
    }

    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &vulkan->swapchain.handle,
        .pImageIndices = &vulkan->imageIndex,
        .waitSemaphoreCount = Array_Count(signalSemaphores),
        .pWaitSemaphores = signalSemaphores,
    };

    if (vulkan->vkQueuePresentKHR(vulkan->presentQueue, &presentInfo) != VK_SUCCESS) {
        printf("ERROR: failed to present present queue.\n");

        return;
    }

    vulkan->vertexCount = 0;
    vulkan->frameIndex = (vulkan->frameIndex + 1) % FRAME_COUNT;
}
