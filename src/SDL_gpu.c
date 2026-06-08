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
    vertex_attributes[1].offset = sizeof(f32) * 3;

    vertex_attributes[2].location = 2;
    vertex_attributes[2].buffer_slot = 0;
    vertex_attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    vertex_attributes[2].offset = sizeof(f32) * 7;

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

bool gpu_upload_texture(SDL_GPUDevice *device, SDL_GPUCopyPass *copy_pass, SDL_GPUTexture *texture, const void *pixels, u32 width, u32 height)
{
    if (!device || !copy_pass || !texture || !pixels)
    {
        return false;
    }

    usize texture_size = width * height * 4;

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

    SDL_GPUTextureTransferInfo destination_transfer_info = {0};
    destination_transfer_info.transfer_buffer = transfer_buffer;
    destination_transfer_info.offset = 0;

    SDL_GPUTextureRegion destination_region = {0};
    destination_region.texture = texture;
    destination_region.w = width;
    destination_region.h = height;
    destination_region.d = 1;

    SDL_UploadToGPUTexture(copy_pass, &destination_transfer_info, &destination_region, false);
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
    bool ok = gpu_upload_texture(gpu->device, copy_pass, gpu->textures[UNTEXTURED], pixels, 1, 1);
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
    u32 vertices = 0;
    usize offset = 0;

    while (offset < render_buffer->current_offset)
    {
        render_entry_header *header = (render_entry_header *)(render_buffer->base_pointer + offset);
        if (header->type == render_entry_type_draw_rectangle ||
            header->type == render_entry_type_draw_line)
        {
            vertices += 6;
        }
        offset += header->size;
    }

    vertices = MIN(vertices, 4096);
    *out_vertex_count = vertices;

    SDL_GPUTransferBuffer *vertex_transfer_buffer = 0;
    vertex *map = 0;

    if (vertices > 0)
    {
        SDL_GPUTransferBufferCreateInfo transfer_info = {0};
        transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        transfer_info.size = vertices * sizeof(vertex);
        vertex_transfer_buffer = SDL_CreateGPUTransferBuffer(gpu->device, &transfer_info);

        if (vertex_transfer_buffer)
        {
            map = (vertex *)SDL_MapGPUTransferBuffer(gpu->device, vertex_transfer_buffer, false);
        }
    }

    int width, height;
    SDL_GetWindowSizeInPixels(gpu->window, &width, &height);
    f32 normalied_width_x = 2.0f / (f32)width;
    f32 normalized_height_y = 2.0f / (f32)height;

    SDL_GPUCopyPass *copy_pass = 0;
    offset = 0;
    u32 current_vertex = 0;

    /****************************************************/

    while (offset < render_buffer->current_offset)
    {
        render_entry_header *header = (render_entry_header *)(render_buffer->base_pointer + offset);

        switch (header->type)
        {
        case render_entry_type_allocate_texture:
        {
            render_entry_allocate_texture *command = (render_entry_allocate_texture *)header;
            bool is_index_valid = (command->index > UNTEXTURED && command->index < ARRAY_COUNT(gpu->textures));

            if (is_index_valid && command->pixels)
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

                    if (gpu_upload_texture(gpu->device, copy_pass, new_texture, command->pixels, command->size.x, command->size.y))
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
        break;

        case render_entry_type_draw_rectangle:
        {
            if (map && current_vertex + 6 <= vertices)
            {
                render_entry_draw_rectangle *command = (render_entry_draw_rectangle *)header;

                f32 left = (command->rectangle.x) * normalied_width_x - 1.0f;
                f32 right = (command->rectangle.x + command->rectangle.width) * normalied_width_x - 1.0f;
                f32 top = 1.0f - (command->rectangle.y) * normalized_height_y;
                f32 bottom = 1.0f - (command->rectangle.y + command->rectangle.height) * normalized_height_y;

                f32 r = command->color.r;
                f32 g = command->color.g;
                f32 b = command->color.b;
                f32 a = command->color.a;

                f32 u_min = command->uv.min.x;
                f32 v_min = command->uv.min.y;
                f32 u_max = command->uv.max.x;
                f32 v_max = command->uv.max.y;

                map[current_vertex++] = (vertex){left, top, 0.0f, r, g, b, a, u_min, v_min};
                map[current_vertex++] = (vertex){right, top, 0.0f, r, g, b, a, u_max, v_min};
                map[current_vertex++] = (vertex){left, bottom, 0.0f, r, g, b, a, u_min, v_max};

                map[current_vertex++] = (vertex){right, top, 0.0f, r, g, b, a, u_max, v_min};
                map[current_vertex++] = (vertex){right, bottom, 0.0f, r, g, b, a, u_max, v_max};
                map[current_vertex++] = (vertex){left, bottom, 0.0f, r, g, b, a, u_min, v_max};
            }
        }
        break;

        case render_entry_type_draw_line:
        {
            if (map && current_vertex + 6 <= vertices)
            {
                render_entry_draw_line *command = (render_entry_draw_line *)header;

                f32 dx = command->end.x - command->start.x;
                f32 dy = command->end.y - command->start.y;
                f32 length = SQRT(dx * dx + dy * dy);

                if (length > 0.0001f)
                {
                    f32 nx = -dy / length;
                    f32 ny = dx / length;

                    f32 tx = nx * 1.0f;
                    f32 ty = ny * 1.0f;

                    f32 cx0 = (command->start.x + tx) * normalied_width_x - 1.0f;
                    f32 cy0 = 1.0f - (command->start.y + ty) * normalized_height_y;

                    f32 cx1 = (command->start.x - tx) * normalied_width_x - 1.0f;
                    f32 cy1 = 1.0f - (command->start.y - ty) * normalized_height_y;

                    f32 cx2 = (command->end.x + tx) * normalied_width_x - 1.0f;
                    f32 cy2 = 1.0f - (command->end.y + ty) * normalized_height_y;

                    f32 cx3 = (command->end.x - tx) * normalied_width_x - 1.0f;
                    f32 cy3 = 1.0f - (command->end.y - ty) * normalized_height_y;

                    f32 r = command->color.r;
                    f32 g = command->color.g;
                    f32 b = command->color.b;
                    f32 a = command->color.a;

                    map[current_vertex++] = (vertex){cx0, cy0, 0.0f, r, g, b, a, 0.0f, 0.0f};
                    map[current_vertex++] = (vertex){cx2, cy2, 0.0f, r, g, b, a, 0.0f, 0.0f};
                    map[current_vertex++] = (vertex){cx1, cy1, 0.0f, r, g, b, a, 0.0f, 0.0f};

                    map[current_vertex++] = (vertex){cx2, cy2, 0.0f, r, g, b, a, 0.0f, 0.0f};
                    map[current_vertex++] = (vertex){cx3, cy3, 0.0f, r, g, b, a, 0.0f, 0.0f};
                    map[current_vertex++] = (vertex){cx1, cy1, 0.0f, r, g, b, a, 0.0f, 0.0f};
                }
            }
        }
        break;

        default:
            break;
        }

        offset += header->size;
    }

    if (vertex_transfer_buffer && map)
    {
        SDL_UnmapGPUTransferBuffer(gpu->device, vertex_transfer_buffer);

        if (!copy_pass)
        {
            copy_pass = SDL_BeginGPUCopyPass(command_buffer);
        }

        SDL_GPUTransferBufferLocation source = {0};
        source.transfer_buffer = vertex_transfer_buffer;

        SDL_GPUBufferRegion destination = {0};
        destination.buffer = gpu->vertex_buffer;
        destination.size = vertices * sizeof(vertex);

        SDL_UploadToGPUBuffer(copy_pass, &source, &destination, false);
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
    color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target_info.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(command_buffer, &color_target_info, 1, 0);
    if (!render_pass)
    {
        return;
    }

    SDL_BindGPUGraphicsPipeline(render_pass, gpu->pipeline);

    SDL_GPUBufferBinding vertex_buffer_binding = {0};
    vertex_buffer_binding.buffer = gpu->vertex_buffer;
    vertex_buffer_binding.offset = 0;
    SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_buffer_binding, 1);

    usize offset = 0;
    u32 current_vertex = 0;

    /****************************************************/

    while (offset < render_buffer->current_offset)
    {
        render_entry_header *header = (render_entry_header *)(render_buffer->base_pointer + offset);

        switch (header->type)
        {
        case render_entry_type_draw_rectangle:
        {
            render_entry_draw_rectangle *command = (render_entry_draw_rectangle *)header;

            if (current_vertex + 6 <= vertices)
            {
                SDL_GPUTexture *texture = gpu->textures[UNTEXTURED];
                bool has_valid_texture = (command->texture > 0 && command->texture < ARRAY_COUNT(gpu->textures) && gpu->textures[command->texture]);

                if (has_valid_texture)
                {
                    texture = gpu->textures[command->texture];
                }

                SDL_GPUTextureSamplerBinding sampler_binding = {0};
                sampler_binding.texture = texture;
                sampler_binding.sampler = gpu->sampler;

                SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);
                SDL_DrawGPUPrimitives(render_pass, 6, 1, current_vertex, 0);

                current_vertex += 6;
            }
        }
        break;

        case render_entry_type_draw_line:
        {
            if (current_vertex + 6 <= vertices)
            {
                SDL_GPUTextureSamplerBinding sampler_binding = {0};
                sampler_binding.texture = gpu->textures[UNTEXTURED];
                sampler_binding.sampler = gpu->sampler;

                SDL_BindGPUFragmentSamplers(render_pass, 0, &sampler_binding, 1);
                SDL_DrawGPUPrimitives(render_pass, 6, 1, current_vertex, 0);

                current_vertex += 6;
            }
        }
        break;

        default:
            break;
        }

        offset += header->size;
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
