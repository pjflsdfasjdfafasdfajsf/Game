
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

bool map_add(map *map, rectangle rectangle);

bool map_write(memory *memory, platform *platform, map *map);

bool map_load(memory *memory, platform *platform, const char *map_file, map *map);
