#include "linux_vulkan.h"
#include "game_platform.h"
#include "game_types.h"
#include "linux.h"

#include <dlfcn.h>

// NOTE: compiled shaders.
#include "basic_geometry_vertex.spv.h"
#include "basic_geometry_pixel.spv.h"

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;

    u32 format_count;
    VkSurfaceFormatKHR formats[32];

    u32 presentModeCount;
    VkPresentModeKHR present_modes[8];
} vulkan_swapchain_support;

static u32 vulkan_memory_type_find(vulkan *vulkan, u32 type_filter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memoryProperties;
    vulkan->vkGetPhysicalDeviceMemoryProperties(vulkan->physical_device, &memoryProperties);

    for (u32 i = 0; i < memoryProperties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    return 0;
}

static vulkan_buffer vulkan_buffer_host_visible_create(vulkan *vulkan, VkDeviceSize size, VkBufferUsageFlags flags, void **mapped) {
    vulkan_buffer result = {
        .size = size,
    };

    VkBufferCreateInfo buffer_create_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .usage = flags,
        .size = size,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateBuffer(vulkan->logical_device, &buffer_create_info, 0, &result.handle));
    VkMemoryRequirements memoryRequirements;
    vulkan->vkGetBufferMemoryRequirements(vulkan->logical_device, result.handle, &memoryRequirements);

    VkMemoryAllocateInfo allocation_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = vulkan_memory_type_find(vulkan, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkAllocateMemory(vulkan->logical_device, &allocation_info, 0, &result.memory));
    THIS_MUST_SUCCEED(vulkan, vulkan->vkBindBufferMemory(vulkan->logical_device, result.handle, result.memory, 0));
    THIS_MUST_SUCCEED(vulkan, vulkan->vkMapMemory(vulkan->logical_device, result.memory, 0, result.size, 0, mapped));
    return result;
}

static void vulkan_buffer_host_visible_destroy(vulkan *vulkan, vulkan_buffer buffer) {
    vulkan->vkUnmapMemory(vulkan->logical_device, buffer.memory);
    vulkan->vkFreeMemory(vulkan->logical_device, buffer.memory, 0);
    vulkan->vkDestroyBuffer(vulkan->logical_device, buffer.handle, 0);
}

static bool vulkan_instance_create(vulkan *vulkan, PFN_vkCreateInstance vkCreateInstance, const char *platform_extension) {
    const char *extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        platform_extension,
#if defined(DEBUG)
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
#endif
    };

#if defined(DEBUG)
    const char *layers[] = {
        "VK_LAYER_KHRONOS_validation",
    };
#endif

    VkApplicationInfo application_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "game",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "miguel engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo instance_create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &application_info,
        .enabledExtensionCount = ARRAY_COUNT(extensions),
        .ppEnabledExtensionNames = extensions,
#if defined(DEBUG)
        .enabledLayerCount = ARRAY_COUNT(layers),
        .ppEnabledLayerNames = layers,
#endif
    };

    VkResult result = vkCreateInstance(&instance_create_info, 0, &vulkan->instance);

#if defined(DEBUG)
    if (result == VK_ERROR_LAYER_NOT_PRESENT || result == VK_ERROR_EXTENSION_NOT_PRESENT) {
        memory_stream_write_string_format(vulkan->info_stream, "warning: validation layers were requested but are not available\n");

        instance_create_info.enabledLayerCount = 0;
        instance_create_info.ppEnabledLayerNames = 0;
        instance_create_info.enabledExtensionCount = ARRAY_COUNT(extensions) - 1;

        result = vkCreateInstance(&instance_create_info, 0, &vulkan->instance);
    } else if (result == VK_SUCCESS) {
        memory_stream_write_string_format(vulkan->info_stream, "validation layers enabled\n");
    }
#endif

    if (result != VK_SUCCESS) {
        memory_stream_write_string_format(vulkan->error_stream, "error: failed to create vulkan instance.\n");
        return false;
    }

    return true;
}

#if defined(DEBUG)

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user_data) {
    UNUSED(severity);
    UNUSED(type);

    vulkan *state = (vulkan *)user_data;
    memory_stream_write_string_format(state->error_stream, "%s\n", data->pMessage);

    return VK_FALSE;
}

static void vulkan_debug_messenger_create(vulkan *vulkan) {
    VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = vulkan_debug_messenger_callback,
        .pUserData = vulkan,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateDebugUtilsMessengerEXT(vulkan->instance, &debug_messenger_create_info, 0, &vulkan->debug_messenger));
}

#endif

static bool vulkan_queue_families_complete(vulkan *vulkan) {
    return (vulkan->queue_families.graphics_index != 0xffffffff && vulkan->queue_families.present_index != 0xffffffff);
}

static void vulkan_queue_families_find(vulkan *vulkan, VkPhysicalDevice physical_device) {
    vulkan->queue_families.graphics_index = 0xffffffff;
    vulkan->queue_families.present_index = 0xffffffff;

    u32 count = 0;
    vulkan->vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, 0);

    count = MIN(count, 16);

    VkQueueFamilyProperties families[16];
    vulkan->vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &count, families);

    for (u32 i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            vulkan->queue_families.graphics_index = i;
        }

        VkBool32 present_support = false;
        vulkan->vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, vulkan->surface, &present_support);
        if (present_support) {
            vulkan->queue_families.present_index = i;
        }

        if (vulkan_queue_families_complete(vulkan)) {
            break;
        }
    }
}

static bool vulkan_physical_device_suitable(vulkan *vulkan, VkPhysicalDevice physical_device) {
    VkPhysicalDeviceProperties properties;
    vulkan->vkGetPhysicalDeviceProperties(physical_device, &properties);

    VkPhysicalDeviceFeatures features;
    vulkan->vkGetPhysicalDeviceFeatures(physical_device, &features);
    vulkan_queue_families_find(vulkan, physical_device);
    return (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && vulkan_queue_families_complete(vulkan));
}

static bool vulkan_physical_device_pick(vulkan *vulkan) {
    u32 count = 0;

    THIS_MUST_SUCCEED(vulkan, vulkan->vkEnumeratePhysicalDevices(vulkan->instance, &count, 0));
    count = MIN(count, 8);

    VkPhysicalDevice devices[8];
    THIS_MUST_SUCCEED(vulkan, vulkan->vkEnumeratePhysicalDevices(vulkan->instance, &count, devices));
    for (u32 i = 0; i < count; i++) {
        if (vulkan_physical_device_suitable(vulkan, devices[i])) {
            vulkan->physical_device = devices[i];

            break;
        }
    }

    if (!vulkan->physical_device) {
        memory_stream_write_string_format(vulkan->error_stream, "warning: I did not find a dedicated GPU.\n");
        vulkan->physical_device = devices[0];
    }

    if (!vulkan->physical_device) {
        memory_stream_write_string_format(vulkan->error_stream, "error: your GPU is not supported by vulkan.\n");

        return false;
    }

    VkPhysicalDeviceProperties properties;
    vulkan->vkGetPhysicalDeviceProperties(vulkan->physical_device, &properties);
    memory_stream_write_string_format(vulkan->info_stream, "gpu: %s\n", properties.deviceName);

    return true;
}

static bool vulkan_logical_device_create(vulkan *vulkan) {
    const char *extensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    f32 queue_priority = 1.0f;

    u32 unique_indices[2];
    u32 unique_count = 0;

    unique_indices[unique_count++] = vulkan->queue_families.graphics_index;
    if (vulkan->queue_families.present_index != vulkan->queue_families.graphics_index) {
        memory_stream_write_string_format(vulkan->info_stream, "graphics and present queue are from diffrent families.\n");

        unique_indices[unique_count++] = vulkan->queue_families.present_index;
    }

    VkDeviceQueueCreateInfo queue_create_infos[2];

    for (u32 i = 0; i < unique_count; i++) {
        queue_create_infos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = unique_indices[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .dynamicRendering = VK_TRUE,
    };

    VkPhysicalDeviceVulkan12Features vulkan12_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &dynamicRendering,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
    };

    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &vulkan12_features,
        .pQueueCreateInfos = queue_create_infos,
        .queueCreateInfoCount = unique_count,
        .enabledExtensionCount = ARRAY_COUNT(extensions),
        .ppEnabledExtensionNames = extensions,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateDevice(vulkan->physical_device, &device_create_info, 0, &vulkan->logical_device));
    return true;
}

static vulkan_swapchain_support vulkan_swapchain_support_query(vulkan *vulkan) {
    vulkan_swapchain_support result = {0};

    THIS_MUST_SUCCEED(vulkan, vulkan->vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vulkan->physical_device, vulkan->surface, &result.capabilities));
    THIS_MUST_SUCCEED(vulkan, vulkan->vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physical_device, vulkan->surface, &result.format_count, 0));
    if (!result.format_count) {
        memory_stream_write_string_format(vulkan->error_stream, "error: failed to get vulkan surface formats.\n");

        return result;
    }

    // NOTE: there will be a segmentation fault if we don't clamp this because drivers can sometimes return just a lot of them.
    result.format_count = MIN(result.format_count, 32);
    THIS_MUST_SUCCEED(vulkan, vulkan->vkGetPhysicalDeviceSurfaceFormatsKHR(vulkan->physical_device, vulkan->surface, &result.format_count, result.formats));
    THIS_MUST_SUCCEED(vulkan, vulkan->vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->physical_device, vulkan->surface, &result.presentModeCount, 0));
    if (!result.presentModeCount) {
        memory_stream_write_string_format(vulkan->error_stream, "error: failed to get vulkan surface present modes.\n");

        return result;
    }
    result.presentModeCount = MIN(result.presentModeCount, 8);
    THIS_MUST_SUCCEED(vulkan, vulkan->vkGetPhysicalDeviceSurfacePresentModesKHR(vulkan->physical_device, vulkan->surface, &result.presentModeCount, result.present_modes));
    return result;
}

static VkSurfaceFormatKHR vulkan_surface_format_select(vulkan_swapchain_support *support) {
    for (u32 i = 0; i < support->format_count; i++) {
        VkSurfaceFormatKHR format = support->formats[i];
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return support->formats[0];
}

static VkPresentModeKHR vulkan_present_mode_select(vulkan_swapchain_support *support) {
    for (u32 i = 0; i < support->presentModeCount; i++) {
        VkPresentModeKHR presentMode = support->present_modes[i];
        if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return presentMode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D vulkan_extent_select(vulkan_swapchain_support *support, u32 width, u32 height) {
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

static bool vulkan_swapchain_create(vulkan *vulkan, u32 window_width, u32 window_height) {
    vulkan_swapchain_support support = vulkan_swapchain_support_query(vulkan);
    VkSurfaceFormatKHR surfaceFormat = vulkan_surface_format_select(&support);
    VkPresentModeKHR presentMode = vulkan_present_mode_select(&support);

    VkExtent2D extent = vulkan_extent_select(&support, (u32)window_width, (u32)window_height);

    u32 image_count = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount) {
        image_count = support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = vulkan->surface,
        .minImageCount = image_count,
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

    u32 queue_family_indices[] = {
        vulkan->queue_families.graphics_index,
        vulkan->queue_families.present_index,
    };

    if (vulkan->queue_families.graphics_index != vulkan->queue_families.present_index) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    };

    vulkan_swapchain *swapchain = &vulkan->swapchain;
    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateSwapchainKHR(vulkan->logical_device, &swapchain_create_info, 0, &swapchain->handle));
    swapchain->format = surfaceFormat.format;
    swapchain->extent = extent;

    THIS_MUST_SUCCEED(vulkan, vulkan->vkGetSwapchainImagesKHR(vulkan->logical_device, swapchain->handle, &swapchain->image_count, 0));
    ASSERT(swapchain->image_count <= VULKAN_SWAPCHAIN_MAX_IMAGE_COUNT);
    THIS_MUST_SUCCEED(vulkan, vulkan->vkGetSwapchainImagesKHR(vulkan->logical_device, swapchain->handle, &swapchain->image_count, swapchain->images));
    for (u32 i = 0; i < swapchain->image_count; i++) {
        VkImageViewCreateInfo image_view_create_info = {
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

        THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateImageView(vulkan->logical_device, &image_view_create_info, 0, &swapchain->image_views[i]));
        VkSemaphoreCreateInfo semaphore_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateSemaphore(vulkan->logical_device, &semaphore_create_info, 0, &swapchain->render_finished_semaphore[i]));
    }

    return true;
}

static void vulkan_swapchain_destroy(vulkan *vulkan) {
    for (u32 i = 0; i < vulkan->swapchain.image_count; i++) {
        vulkan->vkDestroyImageView(vulkan->logical_device, vulkan->swapchain.image_views[i], 0);
        vulkan->vkDestroySemaphore(vulkan->logical_device, vulkan->swapchain.render_finished_semaphore[i], 0);
    }

    vulkan->vkDestroySwapchainKHR(vulkan->logical_device, vulkan->swapchain.handle, 0);
}

static void vulkan_swapchain_recreate(vulkan *vulkan, u32 window_width, u32 window_height) {
    // NOTE: get rid of this wait later for smooth swapchain resizing
    THIS_MUST_SUCCEED(vulkan, vulkan->vkDeviceWaitIdle(vulkan->logical_device));
    vulkan_swapchain_destroy(vulkan);
    vulkan_swapchain_create(vulkan, window_width, window_height);
}

static bool vulkan_frame_resources_create(vulkan *vulkan) {
    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = vulkan->queue_families.graphics_index,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateCommandPool(vulkan->logical_device, &command_pool_create_info, 0, &vulkan->command_pool));
    VkPhysicalDeviceProperties properties;
    vulkan->vkGetPhysicalDeviceProperties(vulkan->physical_device, &properties);
    vulkan->uniform_stride = (sizeof(u32) + properties.limits.minUniformBufferOffsetAlignment - 1) & ~(properties.limits.minUniformBufferOffsetAlignment - 1);

    for (u32 i = 0; i < FRAME_COUNT; i++) {
        vulkan_frame_data *frame_data = &vulkan->frame_data[i];

        VkCommandBufferAllocateInfo allocate_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = vulkan->command_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        THIS_MUST_SUCCEED(vulkan, vulkan->vkAllocateCommandBuffers(vulkan->logical_device, &allocate_info, &frame_data->command_buffer));
        VkSemaphoreCreateInfo semaphore_create_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };

        THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateSemaphore(vulkan->logical_device, &semaphore_create_info, 0, &frame_data->image_available_semaphore));
        VkFenceCreateInfo fence_create_info = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT,
        };

        THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateFence(vulkan->logical_device, &fence_create_info, 0, &frame_data->in_flight_fence));
        frame_data->uniform_buffer = vulkan_buffer_host_visible_create(vulkan, vulkan->uniform_stride * 4096, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, (void **)&frame_data->uniform_data);
        frame_data->uniform_count = 0;
    }

    return true;
}

static bool vulkan_descriptor_sets_create(vulkan *vulkan) {
    VkDescriptorPoolSize pool_sizes[] = {
        {.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = FRAME_COUNT},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = MAX_TEXTURES * FRAME_COUNT},
        {.type = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = FRAME_COUNT},
    };

    VkDescriptorPoolCreateInfo descriptor_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = FRAME_COUNT * 2,
        .poolSizeCount = ARRAY_COUNT(pool_sizes),
        .pPoolSizes = pool_sizes,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateDescriptorPool(vulkan->logical_device, &descriptor_pool_create_info, 0, &vulkan->descriptor_pool));
    VkDescriptorSetLayoutBinding resources_set_layout_binding[] = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
        {.binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, .descriptorCount = MAX_TEXTURES, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
    };

    VkDescriptorBindingFlags resources_descriptor_binding_flags[] = {0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT};
    VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info0 = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = ARRAY_COUNT(resources_descriptor_binding_flags),
        .pBindingFlags = resources_descriptor_binding_flags,
    };

    VkDescriptorSetLayoutCreateInfo resources_descriptor_set_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &flags_info0,
        .bindingCount = ARRAY_COUNT(resources_set_layout_binding),
        .pBindings = resources_set_layout_binding,
    };
    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateDescriptorSetLayout(vulkan->logical_device, &resources_descriptor_set_layout_create_info, 0, &vulkan->resource_descriptor_layout));
    VkDescriptorSetLayoutBinding samplers_set_layout_binding[] = {
        {.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT},
    };

    VkDescriptorSetLayoutCreateInfo samplers_descriptor_set_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = ARRAY_COUNT(samplers_set_layout_binding),
        .pBindings = samplers_set_layout_binding,
    };
    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateDescriptorSetLayout(vulkan->logical_device, &samplers_descriptor_set_layout_create_info, 0, &vulkan->sampler_descriptor_set_layout));
    for (u32 i = 0; i < FRAME_COUNT; i++) {
        VkDescriptorSetLayout descriptor_set_layouts[] = {vulkan->resource_descriptor_layout, vulkan->sampler_descriptor_set_layout};

        VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = vulkan->descriptor_pool,
            .descriptorSetCount = 2,
            .pSetLayouts = descriptor_set_layouts,
        };

        VkDescriptorSet descriptor_sets[2];
        THIS_MUST_SUCCEED(vulkan, vulkan->vkAllocateDescriptorSets(vulkan->logical_device, &descriptor_set_allocate_info, descriptor_sets));
        vulkan->resource_descriptor_sets[i] = descriptor_sets[0];
        vulkan->sampler_descriptor_sets[i] = descriptor_sets[1];

        VkDescriptorBufferInfo descriptor_buffer_create_info = {.buffer = vulkan->frame_data[i].uniform_buffer.handle, .offset = 0, .range = sizeof(u32)};
        VkWriteDescriptorSet write_resources_descriptor_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vulkan->resource_descriptor_sets[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
            .pBufferInfo = &descriptor_buffer_create_info,
        };

        VkDescriptorImageInfo sampler_descriptor_image_info = {.sampler = vulkan->texture_sampler};
        VkWriteDescriptorSet write_sampler_descriptor_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = vulkan->sampler_descriptor_sets[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
            .pImageInfo = &sampler_descriptor_image_info,
        };

        VkWriteDescriptorSet descriptor_set_writes[] = {write_resources_descriptor_set, write_sampler_descriptor_set};
        vulkan->vkUpdateDescriptorSets(vulkan->logical_device, 2, descriptor_set_writes, 0, 0);
    }

    return true;
}

static bool vulkan_graphics_pipeline_create(vulkan *vulkan) {
    VkShaderModuleCreateInfo vertex_shader_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = (const u32 *)global_vertex_shader,
        .codeSize = sizeof(global_vertex_shader),
    };

    VkShaderModuleCreateInfo fragment_shader_create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pCode = (const u32 *)global_pixel_shader,
        .codeSize = sizeof(global_pixel_shader),
    };

    VkShaderModule vertex_module, fragment_module;

    if (vulkan->vkCreateShaderModule(vulkan->logical_device, &vertex_shader_create_info, 0, &vertex_module) != VK_SUCCESS) {
        printf("error: failed to create vulkan vertex shader module.\n");

        return false;
    }
    if (vulkan->vkCreateShaderModule(vulkan->logical_device, &fragment_shader_create_info, 0, &fragment_module) != VK_SUCCESS) {
        printf("error: failed to create vulkan fragment shader module.\n");

        return false;
    }

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertex_module,
            .pName = "VSMain",
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragment_module,
            .pName = "PSMain",
        },
    };

    VkVertexInputBindingDescription binding_description = {
        .binding = 0,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        .stride = sizeof(vertex),
    };

    VkVertexInputAttributeDescription attribute_description[] = {
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

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding_description,
        .vertexAttributeDescriptionCount = ARRAY_COUNT(attribute_description),
        .pVertexAttributeDescriptions = attribute_description,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
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

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_TRUE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = ARRAY_COUNT(dynamic_states),
        .pDynamicStates = dynamic_states,
    };

    VkPipelineRenderingCreateInfo rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &vulkan->swapchain.format,
    };

    VkDescriptorSetLayout descriptor_set_layouts[] = {vulkan->resource_descriptor_layout, vulkan->sampler_descriptor_set_layout};
    VkPipelineLayoutCreateInfo pipeline_layout_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = ARRAY_COUNT(descriptor_set_layouts),
        .pSetLayouts = descriptor_set_layouts,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreatePipelineLayout(vulkan->logical_device, &pipeline_layout_create_info, 0, &vulkan->pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_create_info,
        .stageCount = ARRAY_COUNT(shader_stages),
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rastezier,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = vulkan->pipeline_layout,
        .renderPass = VK_NULL_HANDLE,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateGraphicsPipelines(vulkan->logical_device, VK_NULL_HANDLE, 1, &pipeline_create_info, 0, &vulkan->pipeline));

    vulkan->vkDestroyShaderModule(vulkan->logical_device, vertex_module, 0);
    vulkan->vkDestroyShaderModule(vulkan->logical_device, fragment_module, 0);

    return true;
}

static bool vulkan_sampler_create(vulkan *vulkan) {
    VkSamplerCreateInfo sampler_create_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .maxAnisotropy = 1.0f,
    };

    if (vulkan->vkCreateSampler(vulkan->logical_device, &sampler_create_info, 0, &vulkan->texture_sampler) != VK_SUCCESS) {
        printf("error: failed to create vulkan sampler.\n");

        return false;
    }

    return true;
}

u32 vulkan_texture_create(vulkan *vulkan, u32 index, vector2u size, u32 bytes_per_pixel, const void *pixels) {
    if (vulkan->texture_count + 1 > MAX_TEXTURES) {
        memory_stream_write_string_format(vulkan->error_stream, "error: maximum number of textures reached: %u.\n", vulkan->texture_count);
    }

    vulkan_image result = {0};

    VkImageCreateInfo image_create_info = {
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

    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateImage(vulkan->logical_device, &image_create_info, 0, &result.handle));
    VkMemoryRequirements memoryRequirements;
    vulkan->vkGetImageMemoryRequirements(vulkan->logical_device, result.handle, &memoryRequirements);

    VkMemoryAllocateInfo allocation_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = vulkan_memory_type_find(vulkan, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkAllocateMemory(vulkan->logical_device, &allocation_info, 0, &result.memory));
    THIS_MUST_SUCCEED(vulkan, vulkan->vkBindImageMemory(vulkan->logical_device, result.handle, result.memory, 0));
    VkImageViewCreateInfo image_view_create_info = {
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

    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateImageView(vulkan->logical_device, &image_view_create_info, 0, &result.view));
    u8 *mapped_data;
    vulkan_buffer staging_buffer = vulkan_buffer_host_visible_create(vulkan, size.width * size.height * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, (void **)&mapped_data);

    const u8 *source_pixels = (const u8 *)pixels;
    for (u32 y = 0; y < size.height; ++y) {
        u8 *destination_row = mapped_data + (y * size.width * 4);
        const u8 *source_row = source_pixels + (y * size.width * bytes_per_pixel);

        if (bytes_per_pixel == 4) {
            memory_copy_forwards(destination_row, source_row, size.width * 4);
        } else if (bytes_per_pixel == 3) {
            for (u32 x = 0; x < size.width; ++x) {
                destination_row[x * 4 + 0] = source_row[x * 3 + 0];
                destination_row[x * 4 + 1] = source_row[x * 3 + 1];
                destination_row[x * 4 + 2] = source_row[x * 3 + 2];
                destination_row[x * 4 + 3] = 255;
            }
        }
    }

    VkCommandPool commandPool;
    VkCommandBuffer commandBuffer;

    VkCommandPoolCreateInfo command_pool_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = vulkan->queue_families.graphics_index,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkCreateCommandPool(vulkan->logical_device, &command_pool_create_info, 0, &commandPool));
    VkCommandBufferAllocateInfo command_buffer_allocation_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .commandBufferCount = 1,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkAllocateCommandBuffers(vulkan->logical_device, &command_buffer_allocation_info, &commandBuffer));
    VkCommandBufferBeginInfo command_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkBeginCommandBuffer(commandBuffer, &command_buffer_begin_info));
    VkImageMemoryBarrier image_memory_barrier = {
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

    vulkan->vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 0, 0, 1, &image_memory_barrier);

    VkBufferImageCopy copy_region = {
        .imageExtent = (VkExtent3D){size.width, size.height, 1},
        .imageSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
    };

    vulkan->vkCmdCopyBufferToImage(commandBuffer, staging_buffer.handle, result.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    image_memory_barrier = (VkImageMemoryBarrier){
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

    vulkan->vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, 0, 0, 0, 1, &image_memory_barrier);

    THIS_MUST_SUCCEED(vulkan, vulkan->vkEndCommandBuffer(commandBuffer));
    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkQueueSubmit(vulkan->graphics_queue, 1, &submit_info, VK_NULL_HANDLE));
    THIS_MUST_SUCCEED(vulkan, vulkan->vkQueueWaitIdle(vulkan->graphics_queue));
    vulkan->vkDestroyCommandPool(vulkan->logical_device, commandPool, 0);
    vulkan_buffer_host_visible_destroy(vulkan, staging_buffer);

    vulkan->textures[index] = result;
    vulkan->texture_count++;

    VkDescriptorImageInfo image_info = {
        .sampler = vulkan->texture_sampler,
        .imageView = result.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    VkWriteDescriptorSet descriptor_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .dstBinding = 1,
        .descriptorCount = 1,
        .pImageInfo = &image_info,
        .dstArrayElement = index,
    };

    for (u32 i = 0; i < FRAME_COUNT; i++) {
        descriptor_write.dstSet = vulkan->resource_descriptor_sets[i];
        vulkan->vkUpdateDescriptorSets(vulkan->logical_device, 1, &descriptor_write, 0, 0);
    }

    return index;
}

static bool vulkan_common_initialize(vulkan *vulkan, linux_wayland *window) {
#define INSTANCE_FUNCTION(name) \
    vulkan->name = (PFN_##name)vulkan->vkGetInstanceProcAddr(vulkan->instance, #name);
    VULKAN_INSTANCE_FUNCTIONS
#undef INSTANCE_FUNCTION

#if defined(DEBUG)
#define DEBUG_FUNCTION(name) \
    vulkan->name = (PFN_##name)vulkan->vkGetInstanceProcAddr(vulkan->instance, #name);
    VULKAN_DEBUG_FUNCTIONS
#undef DEBUG_FUNCTION
    if (vulkan->vkCreateDebugUtilsMessengerEXT) {
        vulkan_debug_messenger_create(vulkan);
    }
#endif

    if (!vulkan_physical_device_pick(vulkan)) {
        return false;
    }
    if (!vulkan_logical_device_create(vulkan)) {
        return false;
    }

#define DEVICE_FUNCTION(name) \
    vulkan->name = (PFN_##name)vulkan->vkGetDeviceProcAddr(vulkan->logical_device, #name);
    VULKAN_DEVICE_FUNCTIONS
#undef DEVICE_FUNCTION

    vulkan->vkGetDeviceQueue(vulkan->logical_device, vulkan->queue_families.graphics_index, 0, &vulkan->graphics_queue);
    vulkan->vkGetDeviceQueue(vulkan->logical_device, vulkan->queue_families.present_index, 0, &vulkan->present_queue);

    if (!vulkan_swapchain_create(vulkan, window->width, window->height)) {
        return false;
    }
    if (!vulkan_frame_resources_create(vulkan)) {
        return false;
    }
    if (!vulkan_sampler_create(vulkan)) {
        return false;
    }
    if (!vulkan_descriptor_sets_create(vulkan)) {
        return false;
    }
    if (!vulkan_graphics_pipeline_create(vulkan)) {
        return false;
    }

    u32 white_pixel = 0xFFFFFFFF;
    vulkan_texture_create(vulkan, 0, v2u(1, 1), 4, &white_pixel);

    vulkan->vertex_count = 0;
    vulkan->vertex_capacity = 4096;
    vulkan->vertex_buffer = vulkan_buffer_host_visible_create(vulkan, vulkan->vertex_capacity, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, (void **)&vulkan->vertex_data);

    vulkan->frame_index = 0;
    vulkan->texture_count = 0;

    return true;
}

// NOTE: divide this into windows and linux versions for the future
bool vulkan_initialize(vulkan *vulkan, linux_wayland *window, memory_stream *info_stream, memory_stream *error_stream) {
    ZERO_STRUCT(*vulkan);
    vulkan->info_stream = info_stream;
    vulkan->error_stream = error_stream;

    void *lib_vulkan = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!lib_vulkan) {
        memory_stream_write_string_format(vulkan->error_stream, "error: failed to open libvulkan.so.1: %s\n", dlerror());

        return false;
    }

    PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(lib_vulkan, "vkGetInstanceProcAddr");
    PFN_vkCreateInstance vkCreateInstance = (PFN_vkCreateInstance)vkGetInstanceProcAddr(0, "vkCreateInstance");

    if (!vulkan_instance_create(vulkan, vkCreateInstance, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME)) {
        return false;
    }

    vulkan->vkGetInstanceProcAddr = vkGetInstanceProcAddr;

    PFN_vkCreateWaylandSurfaceKHR vkCreateWaylandSurfaceKHR = (PFN_vkCreateWaylandSurfaceKHR)vkGetInstanceProcAddr(vulkan->instance, "vkCreateWaylandSurfaceKHR");

    VkWaylandSurfaceCreateInfoKHR surface_create_info = {
        .sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
        .display = window->display,
        .surface = window->surface,
    };

    if (vkCreateWaylandSurfaceKHR(vulkan->instance, &surface_create_info, 0, &vulkan->surface) != VK_SUCCESS) {
        memory_stream_write_string_format(vulkan->error_stream, "error: failed to create wayland vulkan surface.\n");

        return false;
    }

    vulkan_common_initialize(vulkan, window);

    return true;
}

static void vulkan_frame_pass_transfer(vulkan *vulkan, const render_command_buffer *commandBuffer) {
    usize memoryOffset = 0;

    while (memoryOffset < commandBuffer->current_offset) {
        render_command_header *header = (render_command_header *)(commandBuffer->base_pointer + memoryOffset);

        if (header->size == 0 || memoryOffset + header->size > commandBuffer->current_offset) {
            break;
        }

        if (header->type == render_command_type_allocate_texture) {
            render_command_allocate_texture *command = (render_command_allocate_texture *)header;
            vulkan_texture_create(vulkan, command->index, command->size, command->bytes_per_pixel, command->pixels);
        }

        memoryOffset += header->size;
    }
}

static void vulkan_frame_pass_render(vulkan *vulkan, const render_command_buffer *commandBuffer) {
    vulkan_frame_data *frame_data = &vulkan->frame_data[vulkan->frame_index];
    usize memoryOffset = 0;

    while (memoryOffset < commandBuffer->current_offset) {
        render_command_header *header = (render_command_header *)(commandBuffer->base_pointer + memoryOffset);
        if (header->size == 0 || memoryOffset + header->size > commandBuffer->current_offset) {
            break;
        }

        switch (header->type) {
        case render_command_type_clear_entire_screen: {
            render_command_clear_entire_screen *command = (render_command_clear_entire_screen *)header;

            vulkan->clear_color = (VkClearValue){
                .color = {{
                    command->color.r,
                    command->color.g,
                    command->color.b,
                    command->color.a,
                }},
            };

        } break;

        case render_command_type_draw_rectangle: {
            render_command_draw_rectangle *command = (render_command_draw_rectangle *)header;

            const u32 vertices_per_rectangle = 6;
            if (vulkan->vertex_count + vertices_per_rectangle <= vulkan->vertex_capacity) {
                f32 left_edge = (command->position.x / (f32)DEFAULT_WINDOW_WIDTH) * 2.0f - 1.0f;
                f32 right_edge = ((command->position.x + command->size.x) / (f32)DEFAULT_WINDOW_WIDTH) * 2.0f - 1.0f;
                f32 top_edge = 1.0f - (command->position.y / (f32)DEFAULT_WINDOW_HEIGHT) * 2.0f;
                f32 bottom_edge = 1.0f - ((command->position.y + command->size.y) / (f32)DEFAULT_WINDOW_HEIGHT) * 2.0f;

                vertex top_left = {{left_edge, top_edge, 0.0f}, {command->color.r, command->color.g, command->color.b, command->color.a}, {command->uv.min.x, command->uv.min.y}};
                vertex top_right = {{right_edge, top_edge, 0.0f}, {command->color.r, command->color.g, command->color.b, command->color.a}, {command->uv.max.x, command->uv.min.y}};
                vertex bottom_left = {{left_edge, bottom_edge, 0.0f}, {command->color.r, command->color.g, command->color.b, command->color.a}, {command->uv.min.x, command->uv.max.y}};
                vertex bottom_right = {{right_edge, bottom_edge, 0.0f}, {command->color.r, command->color.g, command->color.b, command->color.a}, {command->uv.max.x, command->uv.max.y}};

                vertex *current_vertex_destination = (vertex *)vulkan->vertex_data + vulkan->vertex_count;
                current_vertex_destination[0] = top_left;
                current_vertex_destination[1] = top_right;
                current_vertex_destination[2] = bottom_left;
                current_vertex_destination[3] = bottom_left;
                current_vertex_destination[4] = top_right;
                current_vertex_destination[5] = bottom_right;

                VkDeviceSize offset = vulkan->vertex_count * sizeof(vertex);
                vulkan->vkCmdBindVertexBuffers(frame_data->command_buffer, 0, 1, &vulkan->vertex_buffer.handle, &offset);

                u32 *destination = (u32 *)(frame_data->uniform_data + (frame_data->uniform_count * vulkan->uniform_stride));

                *destination = command->texture;

                u32 dynamic_offset = frame_data->uniform_count * vulkan->uniform_stride;
                vulkan->vkCmdBindDescriptorSets(frame_data->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->pipeline_layout, 0, 1, &vulkan->resource_descriptor_sets[vulkan->frame_index], 1, &dynamic_offset);

                vulkan->vkCmdDraw(frame_data->command_buffer, 6, 1, 0, 0);
            }
        } break;
        }

        memoryOffset += header->size;
    }
}

bool vulkan_frame_begin(vulkan *vulkan, linux_wayland *window, render_command_buffer *commandBuffer) {
    vulkan_frame_data *frame_data = &vulkan->frame_data[vulkan->frame_index];

    if (commandBuffer && commandBuffer->base_pointer && !commandBuffer->has_overflowed) {
        vulkan_frame_pass_transfer(vulkan, commandBuffer);
    }

    if (vulkan->swapchain.extent.width != window->width || vulkan->swapchain.extent.height != window->height) {
        vulkan_swapchain_recreate(vulkan, window->width, window->height);
        return false;
    }

    THIS_MUST_SUCCEED(vulkan, vulkan->vkWaitForFences(vulkan->logical_device, 1, &frame_data->in_flight_fence, VK_TRUE, UINT64_MAX));
    VkResult result = vulkan->vkAcquireNextImageKHR(vulkan->logical_device, vulkan->swapchain.handle, UINT64_MAX, frame_data->image_available_semaphore, 0, &vulkan->image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        vulkan_swapchain_recreate(vulkan, window->width, window->height);
        return false;
    }

    THIS_MUST_SUCCEED(vulkan, vulkan->vkResetFences(vulkan->logical_device, 1, &frame_data->in_flight_fence));
    THIS_MUST_SUCCEED(vulkan, vulkan->vkResetCommandBuffer(frame_data->command_buffer, 0));
    VkCommandBufferBeginInfo command_buffer_begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkBeginCommandBuffer(frame_data->command_buffer, &command_buffer_begin_info));
    VkImageMemoryBarrier image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vulkan->swapchain.images[vulkan->image_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
            .levelCount = 1,
        },
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    vulkan->vkCmdPipelineBarrier(frame_data->command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, 0, 0, 0, 1, &image_memory_barrier);

    VkRenderingAttachmentInfo color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = vulkan->swapchain.image_views[vulkan->image_index],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = vulkan->clear_color,
    };

    VkRenderingInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
        .renderArea = {
            .offset = (VkOffset2D){0, 0},
            .extent = vulkan->swapchain.extent,
        },
    };

    vulkan->vkCmdBeginRendering(frame_data->command_buffer, &rendering_info);

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

    vulkan->vkCmdSetViewport(frame_data->command_buffer, 0, 1, &viewport);
    vulkan->vkCmdSetScissor(frame_data->command_buffer, 0, 1, &scissor);

    vulkan->vkCmdBindPipeline(frame_data->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->pipeline);
    vulkan->vkCmdBindDescriptorSets(frame_data->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vulkan->pipeline_layout, 1, 1, &vulkan->sampler_descriptor_sets[vulkan->frame_index], 0, 0);

    frame_data->uniform_count = 0;

    return true;
}

bool vulkan_frame_end(vulkan *vulkan, render_command_buffer *commandBuffer) {
    vulkan_frame_data *frame_data = &vulkan->frame_data[vulkan->frame_index];

    if (commandBuffer && commandBuffer->base_pointer && !commandBuffer->has_overflowed) {
        vulkan_frame_pass_render(vulkan, commandBuffer);
    }

    vulkan->vkCmdEndRendering(frame_data->command_buffer);

    VkImageMemoryBarrier image_memory_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = vulkan->swapchain.images[vulkan->image_index],
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
            .levelCount = 1,
        },
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    vulkan->vkCmdPipelineBarrier(frame_data->command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, 0, 0, 0, 1, &image_memory_barrier);

    THIS_MUST_SUCCEED(vulkan, vulkan->vkEndCommandBuffer(frame_data->command_buffer));
    VkSemaphore wait_semaphores[] = {
        frame_data->image_available_semaphore,
    };

    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    VkSemaphore signal_semaphores[] = {
        vulkan->swapchain.render_finished_semaphore[vulkan->image_index],
    };

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &frame_data->command_buffer,
        .waitSemaphoreCount = ARRAY_COUNT(wait_semaphores),
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .signalSemaphoreCount = ARRAY_COUNT(signal_semaphores),
        .pSignalSemaphores = signal_semaphores,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkQueueSubmit(vulkan->graphics_queue, 1, &submit_info, frame_data->in_flight_fence));
    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .swapchainCount = 1,
        .pSwapchains = &vulkan->swapchain.handle,
        .pImageIndices = &vulkan->image_index,
        .waitSemaphoreCount = ARRAY_COUNT(signal_semaphores),
        .pWaitSemaphores = signal_semaphores,
    };

    THIS_MUST_SUCCEED(vulkan, vulkan->vkQueuePresentKHR(vulkan->present_queue, &present_info));
    vulkan->vertex_count = 0;
    vulkan->frame_index = (vulkan->frame_index + 1) % FRAME_COUNT;

    return true;
}
