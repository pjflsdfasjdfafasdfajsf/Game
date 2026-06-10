
#pragma once

#include "game_platform.h"
#include "game_types.h"

/* NOTE:
 *
 * The ".map" format specification.
 *
 * - [1] one element like this represents a byte
 *   [2, 3] in this case these would represent two bytes with the values 2 and 3
 *
 * - every mutlibyte number is stored in little endian (because we are not ttf or png)
 *
 * The .map format starts with a signatures of 4 bytes being
 * [., m, a, p] 
 * this needs to be the case for EVERY ".map" file
 *
 * The next byte represents the length of the name of the map.
 * The name is not null terminated (so the length doesnt include it as well).
 * [ length of the name ]
 *
 * After that as many bytes as specified for the length of the name follow
 * constructing the map name.
 *
 * The next 4 bytes represent the count of walls in the map
 * [x, x, x, x]
 *
 * After that as many as specified walls follow.
 * Each wall consists of conists of the following pattern:
 * [position x, position y, width, height]
 * [color red, color green, color blue, color alpha]
 *
*/

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
