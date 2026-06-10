
#include "game_map.h"
#include "game_platform.h"
#include "game_types.h"

/* NOTE:
 * this is the signature for .map files. Every .map file
 * needs to have this at the start. */
static u8 map_signature[4] = {'.', 'm', 'a', 'p'};

map map_create(const char *name)
{
	map result = {0};
	
	usize name_length = string_get_length(name);

	/* NOTE: one slot for null terminator */
	name_length = MIN(name_length, ARRAY_COUNT(result.name) - 1);

	memory_copy_forwards(result.name, name, name_length);

	return result;
}

bool map_add(map *map, rectangle bounding_box, vector4 color)
{
	if (!map)
	{
		return false;
	}

	if (map->wall_count + 1 > ARRAY_COUNT(map->walls))
	{
		return false;
	}

	map->walls[map->wall_count++] = (map_wall){
		.bounding_box = bounding_box,
		.color = color,	
	};

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
	memory_stream_write_uint32(&map_stream, *(u32 *)map_signature);

	/* NOTE: name length */
	u8 name_length = (u8)string_get_length(map->name);
	memory_stream_write_uint8(&map_stream, name_length);

	/* NOTE: name */
	for (u8 i = 0; i < name_length; i++) {
		memory_stream_write_uint8(&map_stream, map->name[i]);
	}

	/* NOTE: rectangle count */
	memory_stream_write_uint32(&map_stream, map->wall_count);	

	for (usize i = 0; i < map->wall_count; i++)
	{
		map_wall *wall = &map->walls[i];
		memory_stream_write_f32(&map_stream, wall->bounding_box.x);		
		memory_stream_write_f32(&map_stream, wall->bounding_box.y);		
		memory_stream_write_f32(&map_stream, wall->bounding_box.width);		
		memory_stream_write_f32(&map_stream, wall->bounding_box.height);

		memory_stream_write_f32(&map_stream, wall->color.r);
		memory_stream_write_f32(&map_stream, wall->color.g);
		memory_stream_write_f32(&map_stream, wall->color.b);
		memory_stream_write_f32(&map_stream, wall->color.a);
	}

	platform->file_save(map->name, map_stream.memory, map_stream.offset);

	return true;
}

bool map_load(memory *memory, const void *data, usize size, map *map)
{
	if (!data || !memory || !map)
	{
		return false;
	}

	memory_stream map_stream = {0};
	memory_stream_initialize_read_only(&map_stream, data, size);

	u32 signature = memory_stream_read_uint32_little_endian(&map_stream);
	if (signature != *(u32 *)map_signature)
	{
		memory_stream_write_string_format(memory->standard_error_stream, "ERROR: map signature didnt match for map\n");
		return false;	
	}

	u8 name_length = memory_stream_read_uint8(&map_stream);
	for (u8 i = 0; i < name_length; i++) {
		u8 c = memory_stream_read_uint8(&map_stream);
		map->name[i] = c;
	}

	/* NOTE: guarnteed to have another space for null terminator */
	map->name[name_length] = '\0';

	memory_stream_write_string_format(memory->standard_info_stream, "INFO: Loaded map: %s\n", map->name);

	u32 wall_count = memory_stream_read_uint32_little_endian(&map_stream);
	memory_stream_write_string_format(memory->standard_info_stream, "INFO: Map wall count: %u\n", wall_count);

	map->wall_count = 0;
	
	while (wall_count != 0)
	{
		f32 x = memory_stream_read_f32_little_endian(&map_stream);
		f32 y = memory_stream_read_f32_little_endian(&map_stream);
		f32 width = memory_stream_read_f32_little_endian(&map_stream);
		f32 height = memory_stream_read_f32_little_endian(&map_stream);

		vector4 color;
		color.r = memory_stream_read_f32_little_endian(&map_stream);
		color.g = memory_stream_read_f32_little_endian(&map_stream);
		color.b = memory_stream_read_f32_little_endian(&map_stream);
		color.a = memory_stream_read_f32_little_endian(&map_stream);

		map_wall wall = {
			.bounding_box = rect(v2(x, y), v2(width, height)),
			.color = color,
		};

		map->walls[map->wall_count++] = wall;

		--wall_count;
	}

	return true;		
}
