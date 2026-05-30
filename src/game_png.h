#pragma once

#include "game_platform.h"

typedef struct {
    Vector2U size;
    u32 bytesPerPixel;
    u8 *pixels;
} Image;

Image ImageLoadFromPNG(MemoryArena *permanentArena, MemoryArena *temporaryArena, const void *memory, usize length);
