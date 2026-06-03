#pragma once

#include "game_platform.h"
#include "game_types.h"

typedef struct {
    Vector2U size;
    u32 bytesPerPixel;
    u8 *pixels;
} Image;

Image ImageLoadFromPNG(MemoryArena *permanentArena, MemoryArena *temporaryArena, MemoryStream *errorStream, const void *memory, usize length);
