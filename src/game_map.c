
#include "game_map.h"
#include "game_platform.h"
#include "game_types.h"

/* NOTE:
 * this is the signature for .map files. Every .map file
 * needs to have this at the start.
 */
static const char *map_signature = "MIGUEL\n";

map map_create(const char *name)
{
	map result = {
		.name = name,	
	};

	return result;
}

bool map_add(memory_arena *permanent_arena, map *map, rectangle rectangle)
{
	if (!map || !permanent_arena)
	{
		return false;
	}

	if (map->rectangle_count + 1 > ARRAY_COUNT(map->rectangles))
	{
		return false;
	}

	map->rectangles[map->rectangle_count++] = rectangle;

	return true;
}

bool map_write(platform *platform, memory_arena *temporary_arena, map *map)
{
	if (!platform || !temporary_arena || !map)
	{
		return false;
	}

	/* TODO: handle the max size better */
	usize map_stream_size = 2048;
	void *map_stream_memory = MEMORY_ARENA_PUSH_BYTES(temporary_arena, map_stream_size);	
	memory_stream map_stream = {0};

	memory_stream_initialize_writable(&map_stream, map_stream_memory, map_stream_size);

	memory_stream_write_string(&map_stream, map_signature);	

	for (usize i = 0; i < map->rectangle_count; i++)
	{
		rectangle *rectangle = &map->rectangles[i];
		
		/* NOTE: write rectangle as integers because no float support yet */
		memory_stream_write_string_format(&map_stream, "%d,%d,%d,%d\n", (i32)rectangle->x, (i32)rectangle->y, (i32)rectangle->width, (i32)rectangle->height);	
	}

	platform->file_save(map->name, map_stream.memory, map_stream.offset);

	return true;
}
