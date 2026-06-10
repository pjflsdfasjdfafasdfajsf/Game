
#pragma once

#include "game_platform.h"
#include "game_types.h"

typedef struct
{
	rectangle rectangles[128];
	u32 rectangle_count;

	/* NOTE: store for future reasons like displaying maps in the ui */
	const char *name;
} map;

map map_create(const char *name);

bool map_add(memory_arena *permanent_arena, map *map, rectangle rectangle);

bool map_write(platform *platform, memory_arena *temporary_arena, map *map);
