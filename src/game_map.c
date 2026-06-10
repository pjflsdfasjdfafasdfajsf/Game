
#include "game_map.h"
#include "game_platform.h"
#include "game_types.h"

/* NOTE:
 * this is the signature for .map files. Every .map file
 * needs to have this at the start. */
static u32 map_signature = FOURCC_LE('.', 'm', 'a', 'p');

map map_create(const char *name)
{
	map result = {
		.name = name,	
	};

	return result;
}

bool map_add(map *map, rectangle rectangle)
{
	if (!map)
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

bool map_write(memory *memory, platform *platform, map *map)
{
	if (!platform || !memory || !map)
	{
		return false;
	}

	/* TODO: handle the max size better */
	usize map_stream_size = 2048;
	void *map_stream_memory = MEMORY_ARENA_PUSH_BYTES(&memory->temporary_arena, map_stream_size);	
	memory_stream map_stream = {0};

	memory_stream_initialize_writable(&map_stream, map_stream_memory, map_stream_size);

	/* NOTE: signature */
	memory_stream_write_uint32(&map_stream, map_signature);

	/* NOTE: rectangle count */
	memory_stream_write_uint32(&map_stream, map->rectangle_count);

	for (usize i = 0; i < map->rectangle_count; i++)
	{
		rectangle *rectangle = &map->rectangles[i];
		memory_stream_write_uint32(&map_stream, rectangle->x);		
		memory_stream_write_uint32(&map_stream, rectangle->y);		
		memory_stream_write_uint32(&map_stream, rectangle->width);		
		memory_stream_write_uint32(&map_stream, rectangle->height);		
	}

	platform->file_save(map->name, map_stream.memory, map_stream.offset);

	return true;
}

bool map_load(memory *memory, platform *platform, const char *map_file, map *map)
{
	if (!platform || !memory || !map)
	{
		return false;
	}

	usize map_stream_size = 0; 
	void *map_data = platform->file_load(map_file, &map_stream_size);

	memory_stream map_stream = {0};
	memory_stream_initialize_read_only(&map_stream, map_data, map_stream_size);

	u32 signature = memory_stream_read_uint32_little_endian(&map_stream);
	if (signature != map_signature)
	{
		memory_stream_write_string_format(memory->standard_error_stream, "ERROR: map signature didnt match for map %s\n", map_file);
		return false;	
	}

	u32 rectangle_count = memory_stream_read_uint32_little_endian(&map_stream);
	memory_stream_write_string_format(memory->standard_info_stream, "INFO: Map rectangle count: %u\n", rectangle_count);

	map->name = map_file;
	map->rectangle_count = 0;
	
	while (rectangle_count != 0)
	{
		u32 x = memory_stream_read_uint32_little_endian(&map_stream);
		u32 y = memory_stream_read_uint32_little_endian(&map_stream);
		u32 width = memory_stream_read_uint32_little_endian(&map_stream);
		u32 height = memory_stream_read_uint32_little_endian(&map_stream);

		map->rectangles[map->rectangle_count++] = rect(v2(x, y), v2(width, height));

		--rectangle_count;
	}

	platform->file_free(map_data);	

	return true;		
}
