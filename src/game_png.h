#pragma once

#include "game_platform.h"

typedef struct {
    UnsignedVector2 size;
    u32 bytesPerPixel;
    u8 *pixels;
} Image;

Image ImageLoadFromPNG(const void *memory, usize length);
