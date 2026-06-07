#pragma once

#include "game_platform.h"
#include "game_types.h"

typedef struct
{
    vector2u *points;
    u32 maximum_width;
    u32 maximum_height;
    u32 point_count;
    bool is_initialized;
} pack2d;

pack2d pack2d_create(memory_arena *arena, u32 maximum_width, u32 maximum_height);

bool pack2d_add(pack2d *packer, u32 rectangle_width, u32 rectangle_height, u32 *out_position_x, u32 *out_position_y);
