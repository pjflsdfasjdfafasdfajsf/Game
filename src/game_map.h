
#pragma once

#include "game_platform.h"
#include "game_types.h"

typedef struct
{
	rectangle bounding_box;
	vector4 color;
} map_wall;

typedef struct
{
	map_wall walls[128];
	u32 wall_count;

	/* NOTE: store for future reasons like displaying maps in the ui */
	char name[64];
} map;

map map_create(const char *name);

bool map_add(map *map, rectangle bounding_box, vector4 color);

bool map_write(memory *memory, platform *platform, map *map);

bool map_load(memory *memory, const void *data, usize size, map *map);
