#pragma once

#include "game_platform.h"
#include "game_types.h"

typedef struct {
    vector2u size;
    u32 bytes_per_pixel;
    u8 *pixels;
} image;

image image_load_from_png(memory_arena *permanent_arena, memory_arena *temporary_arena, memory_stream *error_stream, const void *memory, usize length);
