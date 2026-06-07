#include "SDL_gpu.h"

bool gpu_device_initialize(gpu *gpu)
{
    /** TODO: use shadercross */
    gpu->device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, 0);
    if (!gpu->device)
    {
        SDL_Log("error: could not create GPU device: %s\n", SDL_GetError());
        return false;
    }

    SDL_Log("SDL GPU driver: %s\n", SDL_GetGPUDeviceDriver(gpu->device));

    if (!SDL_ClaimWindowForGPUDevice(gpu->device, gpu->window))
    {
        SDL_Log("error: could not claim window: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

bool gpu_pipeline_initialize(gpu *gpu)
{
    SDL_GPUShader *vertex_shader = 0;
    {
        static const unsigned char code[] = {
#embed "shaders/regular.vert.spv"
        };

        SDL_GPUShaderCreateInfo shader_create_info = {0};
        shader_create_info.code_size = sizeof(code);
        shader_create_info.code = code;
        shader_create_info.entrypoint = "main";
        shader_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        shader_create_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;

        vertex_shader = SDL_CreateGPUShader(gpu->device, &shader_create_info);
        if (!vertex_shader)
        {
            SDL_Log("error: could not create vertex shader: %s\n", SDL_GetError());
            return false;
        }
    }

    SDL_GPUShader *fragment_shader = 0;
    {
        static const unsigned char code[] = {
#embed "shaders/regular.frag.spv"
        };

        SDL_GPUShaderCreateInfo shader_create_info = {0};
        shader_create_info.code_size = sizeof(code);
        shader_create_info.code = code;
        shader_create_info.entrypoint = "main";
        shader_create_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        shader_create_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        shader_create_info.num_samplers = 1;

        fragment_shader = SDL_CreateGPUShader(gpu->device, &shader_create_info);
        if (!fragment_shader)
        {
            SDL_Log("error: could not create fragment shader: %s\n", SDL_GetError());
            return false;
        }
    }

    SDL_GPUColorTargetDescription color_target_description = {0};
    color_target_description.format = SDL_GetGPUSwapchainTextureFormat(gpu->device, gpu->window);
    color_target_description.blend_state.enable_blend = true;
    color_target_description.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target_description.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target_description.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_target_description.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_target_description.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_target_description.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    SDL_GPUVertexBufferDescription vertex_buffer_description = {0};
    vertex_buffer_description.slot = 0;
    vertex_buffer_description.pitch = sizeof(vertex);
    vertex_buffer_description.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertex_buffer_description.instance_step_rate = 0;

    SDL_GPUVertexAttribute vertex_attributes[3] = {0};
    vertex_attributes[0].location = 0;
    vertex_attributes[0].buffer_slot = 0;
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].offset = 0;

    vertex_attributes[1].location = 1;
    vertex_attributes[1].buffer_slot = 0;
    vertex_attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    vertex_attributes[1].offset = sizeof(float) * 3;

    vertex_attributes[2].location = 2;
    vertex_attributes[2].buffer_slot = 0;
    vertex_attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[2].offset = sizeof(float) * 7;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_create_info = {0};
    pipeline_create_info.vertex_shader = vertex_shader;
    pipeline_create_info.fragment_shader = fragment_shader;
    pipeline_create_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    pipeline_create_info.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer_description;
    pipeline_create_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_create_info.vertex_input_state.vertex_attributes = vertex_attributes;
    pipeline_create_info.vertex_input_state.num_vertex_attributes = 3;

    pipeline_create_info.target_info.num_color_targets = 1;
    pipeline_create_info.target_info.color_target_descriptions = &color_target_description;

    gpu->pipeline = SDL_CreateGPUGraphicsPipeline(gpu->device, &pipeline_create_info);

    SDL_ReleaseGPUShader(gpu->device, vertex_shader);
    SDL_ReleaseGPUShader(gpu->device, fragment_shader);

    if (!gpu->pipeline)
    {
        SDL_Log("error: could not create graphics pipeline: %s\n", SDL_GetError());
        return false;
    }

    return true;
}

bool gpu_buffers_initialize(gpu *gpu)
{
    SDL_GPUBufferCreateInfo vertex_buffer_create_info = {0};
    vertex_buffer_create_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vertex_buffer_create_info.size = sizeof(vertex) * 4096;

    gpu->vertex_buffer = SDL_CreateGPUBuffer(gpu->device, &vertex_buffer_create_info);
    if (!gpu->vertex_buffer)
    {
        SDL_Log("error: could not create vertex buffer: %s\n", SDL_GetError());
        return false;
    }

    SDL_GPUSamplerCreateInfo sampler_create_info = {0};
    sampler_create_info.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_create_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_create_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_create_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_create_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_create_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    gpu->sampler = SDL_CreateGPUSampler(gpu->device, &sampler_create_info);

    return true;
}

bool gpu_upload_texture(SDL_GPUDevice *device, SDL_GPUCopyPass *copy_pass, SDL_GPUTexture *texture, const void *pixels, u32 width, u32 height, u32 bytes_per_pixel)
{
    if (!device || !copy_pass || !texture || !pixels)
    {
        return false;
    }

    usize texture_size = width * height * bytes_per_pixel;

    SDL_GPUTransferBufferCreateInfo transfer_buffer_create_info = {0};
    transfer_buffer_create_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_buffer_create_info.size = texture_size;

    SDL_GPUTransferBuffer *transfer_buffer = SDL_CreateGPUTransferBuffer(device, &transfer_buffer_create_info);
    if (!transfer_buffer)
    {
        return false;
    }

    void *mapped_memory = SDL_MapGPUTransferBuffer(device, transfer_buffer, false);
    if (!mapped_memory)
    {
        SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);
        return false;
    }

    memory_copy_forwards(mapped_memory, pixels, texture_size);
    SDL_UnmapGPUTransferBuffer(device, transfer_buffer);

    SDL_GPUTextureTransferInfo dest_info = {0};
    dest_info.transfer_buffer = transfer_buffer;
    dest_info.offset = 0;

    SDL_GPUTextureRegion dest_region = {0};
    dest_region.texture = texture;
    dest_region.w = width;
    dest_region.h = height;
    dest_region.d = 1;

    SDL_UploadToGPUTexture(copy_pass, &dest_info, &dest_region, false);
    SDL_ReleaseGPUTransferBuffer(device, transfer_buffer);

    return true;
}

bool gpu_magic_pixel_create(gpu *gpu)
{
    SDL_GPUTextureCreateInfo texture_info = {0};
    texture_info.type = SDL_GPU_TEXTURETYPE_2D;
    texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texture_info.width = 1;
    texture_info.height = 1;
    texture_info.layer_count_or_depth = 1;
    texture_info.num_levels = 1;
    texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

    gpu->textures[UNTEXTURED] = SDL_CreateGPUTexture(gpu->device, &texture_info);
    if (!gpu->textures[UNTEXTURED])
    {
        SDL_Log("error: could not create magic pixel texture: %s\n", SDL_GetError());
        return false;
    }

    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu->device);
    if (!command_buffer)
    {
        SDL_Log("error: could not acquire command buffer for magic pixel: %s\n", SDL_GetError());
        return false;
    }

    static const u8 pixels[4] = {255, 255, 255, 255};

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(command_buffer);
    bool ok = gpu_upload_texture(gpu->device, copy_pass, gpu->textures[UNTEXTURED], pixels, 1, 1, 4);
    SDL_EndGPUCopyPass(copy_pass);

    SDL_SubmitGPUCommandBuffer(command_buffer);

    if (!ok)
    {
        SDL_Log("error: could not upload magic pixel texture\n");
        return false;
    }

    return true;
}

bool gpu_initialize(gpu *gpu, SDL_Window *window)
{
    if (!gpu || !window)
    {
        return false;
    }

    memory_zero(gpu, sizeof(gpu));
    gpu->window = window;

    if (!gpu_device_initialize(gpu))
    {
        return false;
    }
    if (!gpu_pipeline_initialize(gpu))
    {
        return false;
    }
    if (!gpu_buffers_initialize(gpu))
    {
        return false;
    }
    if (!gpu_magic_pixel_create(gpu))
    {
        return false;
    }

    return true;
}

void gpu_pass_upload(gpu *gpu, render_buffer *render_buffer, SDL_GPUCommandBuffer *command_buffer, u32 *out_vertex_count)
{
    SDL_GPUCopyPass *copy_pass = 0;
    usize current_offset = 0;
    u32 vertices = 0;

    while (current_offset < render_buffer->current_offset)
    {
        render_entry_header *header = (render_entry_header *)(render_buffer->base_pointer + current_offset);
        if (header->type == render_entry_type_draw_rectangle)
        {
            vertices += 6;
        }
        current_offset += header->size;
    }

    vertices = MIN(vertices, 4096);
    *out_vertex_count = vertices;

    SDL_GPUTransferBuffer *vertex_transfer_buffer = 0;
    vertex *map = 0;

    if (vertices > 0)
    {
        SDL_GPUTransferBufferCreateInfo transfer_buffer_create_info = {0};
        transfer_buffer_create_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_buffer_create_info.size = vertices * sizeof(vertex);
        vertex_transfer_buffer = SDL_CreateGPUTransferBuffer(gpu->device, &transfer_buffer_create_info);

        if (vertex_transfer_buffer)
        {
            map = (vertex *)SDL_MapGPUTransferBuffer(gpu->device, vertex_transfer_buffer, false);
        }
    }

    int width, height;
    SDL_GetWindowSizeInPixels(gpu->window, &width, &height);
    float pixels_to_ndc_x = 2.0f / (float)width;
    float pixels_to_ndc_y = 2.0f / (float)height;

    current_offset = 0;
    u32 current_vertex = 0;

    while (current_offset < render_buffer->current_offset)
    {
        render_entry_header *header = (render_entry_header *)(render_buffer->base_pointer + current_offset);
        usize next_offset = current_offset + header->size;

        if (header->type == render_entry_type_allocate_texture)
        {
            render_entry_allocate_texture *command = (render_entry_allocate_texture *)header;
            if (command->index > UNTEXTURED && command->index < ARRAY_COUNT(gpu->textures) && command->pixels != 0)
            {
                SDL_GPUTextureCreateInfo texture_info = {0};
                texture_info.type = SDL_GPU_TEXTURETYPE_2D;
                texture_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
                texture_info.width = command->size.x;
                texture_info.height = command->size.y;
                texture_info.layer_count_or_depth = 1;
                texture_info.num_levels = 1;
                texture_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

                SDL_GPUTexture *new_texture = SDL_CreateGPUTexture(gpu->device, &texture_info);
                if (new_texture)
                {
                    if (!copy_pass)
                    {
                        copy_pass = SDL_BeginGPUCopyPass(command_buffer);
                    }

                    if (gpu_upload_texture(gpu->device, copy_pass, new_texture, command->pixels, command->size.x, command->size.y, command->))
                    {
                        if (gpu->textures[command->index])
                        {
                            SDL_ReleaseGPUTexture(gpu->device, gpu->textures[command->index]);
                        }
                        gpu->textures[command->index] = new_texture;
                    }
                    else
                    {
                        SDL_ReleaseGPUTexture(gpu->device, new_texture);
                    }
                }
            }
        }
        else if (header->type == render_entry_type_draw_rectangle)
        {
            if (map && current_vertex + 6 <= vertices)
            {
                render_entry_draw_rectangle *command = (render_entry_draw_rectangle *)header;

                float left = command->rectangle.x * pixels_to_ndc_x - 1.0f;
                float right = (command->rectangle.x + command->rectangle.width) * pixels_to_ndc_x - 1.0f;
                float top = 1.0f - command->rectangle.y * pixels_to_ndc_y;
                float bottom = 1.0f - (command->rectangle.y + command->rectangle.height) * pixels_to_ndc_y;

                map[current_vertex++] = (vertex){left, top, 0.0f, command->color.r, command->color.g, command->color.b, command->color.a, command->uv.min.x, command->uv.min.y};
                map[current_vertex++] = (vertex){right, top, 0.0f, command->color.r, command->color.g, command->color.b, command->color.a, command->uv.max.x, command->uv.min.y};
                map[current_vertex++] = (vertex){left, bottom, 0.0f, command->color.r, command->color.g, command->color.b, command->color.a, command->uv.min.x, command->uv.max.y};
                map[current_vertex++] = (vertex){right, top, 0.0f, command->color.r, command->color.g, command->color.b, command->color.a, command->uv.max.x, command->uv.min.y};
                map[current_vertex++] = (vertex){right, bottom, 0.0f, command->color.r, command->color.g, command->color.b, command->color.a, command->uv.max.x, command->uv.max.y};
                map[current_vertex++] = (vertex){left, bottom, 0.0f, command->color.r, command->color.g, command->color.b, command->color.a, command->uv.min.x, command->uv.max.y};
            }
        }

        current_offset = next_offset;
    }

    if (vertex_transfer_buffer && map)
    {
        SDL_UnmapGPUTransferBuffer(gpu->device, vertex_transfer_buffer);

        if (!copy_pass)
        {
            copy_pass = SDL_BeginGPUCopyPass(command_buffer);
        }

        SDL_GPUTransferBufferLocation source_location = {0};
        source_location.transfer_buffer = vertex_transfer_buffer;

        SDL_GPUBufferRegion destination_region = {0};
        destination_region.buffer = gpu->vertex_buffer;
        destination_region.size = vertices * sizeof(vertex);

        SDL_UploadToGPUBuffer(copy_pass, &source_location, &destination_region, false);
    }

    if (copy_pass)
    {
        SDL_EndGPUCopyPass(copy_pass);
    }
    if (vertex_transfer_buffer)
    {
        SDL_ReleaseGPUTransferBuffer(gpu->device, vertex_transfer_buffer);
    }
}

void gpu_pass_render(gpu *gpu, render_buffer *render_buffer, SDL_GPUCommandBuffer *command_buffer, u32 vertices)
{
    SDL_GPUTexture *swapchain_texture = 0;
    if (!SDL_AcquireGPUSwapchainTexture(command_buffer, gpu->window, &swapchain_texture, 0, 0))
    {
        return;
    }
    if (!swapchain_texture)
    {
        return;
    }

    SDL_GPUColorTargetInfo color_target_info = {0};
    color_target_info.texture = swapchain_texture;
    color_target_info.clear_color.r = 0.0f;
    color_target_info.clear_color.g = 0.0f;
    color_target_info.clear_color.b = 0.0f;
    color_target_info.clear_color.a = 1.0f;
    color_target_info.load_op = SDL_GPU_LOADOP_LOAD;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;

    usize current_offset = 0;
    while (current_offset < render_buffer->current_offset)
    {
        render_entry_header *header = (render_entry_header *)(render_buffer->base_pointer + current_offset);
        if (header->type == render_entry_type_clear_entire_screen)
        {
            render_entry_clear_entire_screen *command = (render_entry_clear_entire_screen *)header;
            color_target_info.clear_color = (SDL_FColor){command->color.r, command->color.g, command->color.b, command->color.a};
            color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
            break;
        }
        current_offset += header->size;
    }

    SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, 0);
    if (!render_pass)
        return;

    SDL_BindGPUGraphicsPipeline(render_pass, gpu->pipeline);

    SDL_GPUBufferBinding vertex_binding = {0};
    vertex_binding.buffer = gpu->vertex_buffer;
    vertex_binding.offset = 0;
    SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);

    current_offset = 0;
    u32 current_vertex_offset = 0;

    while (current_offset < render_buffer->current_offset)
    {
        render_entry_header *header = (render_entry_header *)(render_buffer->base_pointer + current_offset);

        if (header->type == render_entry_type_draw_rectangle)
        {
            render_entry_draw_rectangle *command = (render_entry_draw_rectangle *)header;

            if (current_vertex_offset + 6 <= vertices)
            {
                SDL_GPUTexture *texture = gpu->textures[UNTEXTURED];
                if (command->texture > 0 && command->texture < ARRAY_COUNT(gpu->textures) && gpu->textures[command->texture])
                {
                    texture = gpu->textures[command->texture];
                }

                SDL_GPUTextureSamplerBinding sampler_binding = {0};
                sampler_binding.texture = texture;
                sampler_binding.sampler = gpu->sampler;
                SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);

                SDL_DrawGPUPrimitives(render_pass, 6, 1, current_vertex_offset, 0);
                current_vertex_offset += 6;
            }
        }

        current_offset += header->size;
    }

    SDL_EndGPURenderPass(render_pass);
}

void gpu_update(gpu *gpu, render_buffer *render_buffer)
{
    if (!gpu || !gpu->device || !render_buffer || render_buffer->command_count == 0)
    {
        return;
    }

    SDL_GPUCommandBuffer *command_buffer = SDL_AcquireGPUCommandBuffer(gpu->device);
    if (!command_buffer)
    {
        SDL_Log("error: failed to acquire command buffer: %s\n", SDL_GetError());
        return;
    }

    u32 vertices = 0;
    gpu_pass_upload(gpu, render_buffer, command_buffer, &vertices);
    gpu_pass_render(gpu, render_buffer, command_buffer, vertices);

    SDL_SubmitGPUCommandBuffer(command_buffer);
}
