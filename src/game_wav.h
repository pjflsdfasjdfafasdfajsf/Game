#pragma once

#include "game_platform.h"
#include "game_types.h"

typedef struct
{
    u32 sample_rate;
    u32 sample_count;
    u16 channel_count;
    u16 bits_per_sample;
    void *samples;
} sound_clip;

sound_clip sound_load_from_wav(memory_arena *permanent_arena, memory_stream *error_stream, const void *memory, usize length);
