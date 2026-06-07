#pragma once

#include "game_platform.h" 
#include "game_types.h"

#include <SDL3/SDL.h>

typedef struct
{
    SDL_GPUDevice *device;
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_Window *window;

    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUTexture *textures[4096];
    SDL_GPUSampler *sampler;
} gpu;

bool gpu_initialize(gpu *gpu, SDL_Window *window);
void gpu_update(gpu *gpu, render_buffer *render_buffer);