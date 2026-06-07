#include "game_ttf.h"
#include "game_platform.h"
#include "game_png.h"
#include "game_rectangle_pack.h"
#include "game_types.h"

// NOTE: you can find the spec here:
// https://developer.apple.com/fonts/true_type-reference-manual
//

// NOTE: table parsing.

typedef struct
{
    u32 version;
    u32 font_revision;
    u32 check_sum_adjustment;
    u32 magic_number;
    u16 flags;
    u16 units_per_em;
    i64 created;
    i64 modified;
    i16 x_min;
    i16 y_min;
    i16 x_max;
    i16 y_max;
    u16 mac_style;
    u16 lowest_rec_ppem;
    i16 font_direction_hint;
    i16 index_to_loc_format;
    i16 glyph_data_format;
} true_type_head_table;

typedef struct
{
    u32 version;
    u16 num_glyphs;
    // NOTE: the following fields are only valid if version is 0x00010000 (1.0).
    u16 max_points;
    u16 max_contours;
    u16 max_component_points;
    u16 max_component_contours;
    u16 max_zones;
    u16 max_twilight_points;
    u16 max_storage;
    u16 max_function_defs;
    u16 max_instruction_defs;
    u16 max_stack_elements;
    u16 max_size_of_instructions;
    u16 max_component_elements;
    u16 max_component_depth;
} true_type_maximum_profile_table;

typedef struct
{
    bool is_valid;
    u16 seg_count;
    usize end_code_offset;
    usize start_code_offset;
    usize id_delta_offset;
    usize id_range_offset_offset;
    const u8 *memory;
    usize length;
} true_type_cmap_format4;

typedef struct
{
    bool is_valid;
    i16 index_to_loc_format;
    u16 num_glyphs;
    const u8 *memory;
    usize length;
} true_type_index_to_location_table;

typedef struct
{
    // NOTE: this is relative to the start of the 'glyf' table
    u32 offset;
    u32 length;
} true_type_glyph_location;

typedef struct
{
    i16 x;
    i16 y;
    bool is_on_curve;
} true_type_glyph_point;

typedef struct
{
    bool is_valid;
    // NOTE: they are rejected :)
    bool is_compound;

    i16 number_of_contours;
    i16 x_min;
    i16 y_min;
    i16 x_max;
    i16 y_max;

    u32 number_of_points;
    u16 *end_points_of_contours;

    u16 instruction_length;
    const u8 *instructions;

    true_type_glyph_point *points;
} true_type_simple_glyph;

// NOTE: glyph outline flags
enum
{
    true_type_glyph_flag_on_curve = (1 << 0),
    true_type_glyph_flag_x_short_vector = (1 << 1),
    true_type_glyph_flag_y_short_vector = (1 << 2),
    true_type_glyph_flag_repeat = (1 << 3),
    true_type_glyph_flag_x_is_same_or_positive_x_short = (1 << 4),
    true_type_glyph_flag_y_is_same_or_positive_y_short = (1 << 5),
};

// NOTE: table tags
enum
{
    true_type_table_tag_head = FOURCC('h', 'e', 'a', 'd'),
    true_type_table_tag_maxp = FOURCC('m', 'a', 'x', 'p'),
    true_type_table_tag_cmap = FOURCC('c', 'm', 'a', 'p'),
    true_type_table_tag_loca = FOURCC('l', 'o', 'c', 'a'),
    true_type_table_tag_glyf = FOURCC('g', 'l', 'y', 'f'),
};

typedef struct
{
    vector2 p1;
    vector2 p2;
} true_type_edge;

typedef struct
{
    f32 x;
    i32 direction;
} true_type_scanline_intersection;

static u32 true_type_calculate_table_checksum(const u8 *table_memory, u32 table_length, bool is_head_table)
{
    u32 checksum = 0;
    u32 number_of_longs = (table_length + 3) / 4;
    usize offset = 0;

    for (u32 index = 0; index < number_of_longs; index++)
    {
        u8 byte0 = (offset < table_length) ? table_memory[offset++] : 0;
        u8 byte1 = (offset < table_length) ? table_memory[offset++] : 0;
        u8 byte2 = (offset < table_length) ? table_memory[offset++] : 0;
        u8 byte3 = (offset < table_length) ? table_memory[offset++] : 0;

        u32 value = ((u32)byte0 << 24) | ((u32)byte1 << 16) | ((u32)byte2 << 8) | ((u32)byte3 << 0);

        if (is_head_table && index == 2)
        {
            value = 0;
        }

        checksum += value;
    }

    return checksum;
}

true_type_font true_type_font_load_from_memory(memory_arena *arena, memory_stream *error_stream, const void *memory, usize length)
{
    true_type_font result = {0};

    if (!memory)
    {
        memory_stream_write_string(error_stream, "error: invalid parameter: memory.");

        return result;
    }

    const usize true_type_offset_subtable_length = 12;
    if (length < true_type_offset_subtable_length)
    {
        memory_stream_write_string(error_stream, "error: font file too small to contain offset subtable.");

        return result;
    }

    memory_stream stream;
    memory_stream_initialize_read_only(&stream, memory, length);

    u32 scaler_type_value = memory_stream_read_uint32_big_endian(&stream);

    if (scaler_type_value == 0x00010000)
    {
        result.scaler_type = true_type_scaler_type_windows;
    }
    else if (scaler_type_value == FOURCC('t', 'r', 'u', 'e'))
    {
        result.scaler_type = true_type_scaler_type_mac;
    }
    else if (scaler_type_value == FOURCC('t', 'y', 'p', '1'))
    {
        result.scaler_type = true_type_scaler_type_post_script;
    }
    else if (scaler_type_value == FOURCC('O', 'T', 'T', 'O'))
    {
        result.scaler_type = true_type_scaler_type_open_type;
    }
    else
    {
        memory_stream_write_string(error_stream, "error: unrecognized scaler type; not a "
                                                 "supported true_type/open_type font.");

        return result;
    }

    result.number_of_tables = memory_stream_read_uint16_big_endian(&stream);
    result.search_range = memory_stream_read_uint16_big_endian(&stream);
    result.entry_selector = memory_stream_read_uint16_big_endian(&stream);
    result.range_shift = memory_stream_read_uint16_big_endian(&stream);

    const usize true_type_table_directory_entry_length = 16;
    usize expected_directory_size = true_type_offset_subtable_length + ((usize)result.number_of_tables * true_type_table_directory_entry_length);

    if (length < expected_directory_size)
    {
        memory_stream_write_string(error_stream, "error: font file too small to contain full table directory.");

        return result;
    }

    if (result.number_of_tables > 0)
    {
        result.tables = MEMORY_ARENA_PUSH_ARRAY(arena, true_type_table_directory_entry, result.number_of_tables);
        if (!result.tables)
        {
            memory_stream_write_string(error_stream, "error: out of memory.");

            return result;
        }

        for (u16 table_index = 0; table_index < result.number_of_tables; table_index++)
        {
            const u8 *entry_pointer = &stream.memory[stream.offset];
            stream.offset += true_type_table_directory_entry_length;

            u32 tag = read_uint32_big_endian(&entry_pointer[0]);
            u32 expected_checksum = read_uint32_big_endian(&entry_pointer[4]);
            u32 table_offset = read_uint32_big_endian(&entry_pointer[8]);
            u32 table_length = read_uint32_big_endian(&entry_pointer[12]);

            if (table_offset > length || table_length > (length - table_offset))
            {
                memory_stream_write_string(error_stream, "error: table offset/length out of bounds.");
                RETURN_ZEROED(result);
            }

            const u8 *table_data_pointer = &stream.memory[table_offset];
            bool is_head_table = (tag == true_type_table_tag_head);

            u32 calculated_checksum = true_type_calculate_table_checksum(table_data_pointer, table_length, is_head_table);

            if (calculated_checksum != expected_checksum)
            {
                memory_stream_write_string(error_stream, "error: table checksum mismatch.");
                RETURN_ZEROED(result);
            }

            result.tables[table_index].tag = tag;
            result.tables[table_index].check_sum = expected_checksum;
            result.tables[table_index].offset = table_offset;
            result.tables[table_index].length = table_length;
            result.tables[table_index].memory = table_data_pointer;
        }
    }

    return result;
}

const true_type_table_directory_entry *true_type_font_get_table(const true_type_font *font, u32 table_tag)
{
    if (!font || !font->tables)
    {
        return 0;
    }

    for (u16 table_index = 0; table_index < font->number_of_tables; table_index++)
    {
        if (font->tables[table_index].tag == table_tag)
        {
            return &font->tables[table_index];
        }
    }

    return 0;
}

true_type_head_table true_type_head_table_parse(const true_type_table_directory_entry *head_table_entry)
{
    true_type_head_table result = {0};

    if (!head_table_entry || !head_table_entry->memory)
    {
        return result;
    }

    const usize true_type_head_table_expected_length = 54;
    if (head_table_entry->length < true_type_head_table_expected_length)
    {
        return result;
    }

    memory_stream stream;
    memory_stream_initialize_read_only(&stream, head_table_entry->memory, head_table_entry->length);

    result.version = memory_stream_read_uint32_big_endian(&stream);
    result.font_revision = memory_stream_read_uint32_big_endian(&stream);
    result.check_sum_adjustment = memory_stream_read_uint32_big_endian(&stream);
    result.magic_number = memory_stream_read_uint32_big_endian(&stream);

    const u32 true_type_head_table_magic_number = 0x5F0F3CF5;
    if (result.magic_number != true_type_head_table_magic_number)
    {
        RETURN_ZEROED(result);
    }

    result.flags = memory_stream_read_uint16_big_endian(&stream);
    result.units_per_em = memory_stream_read_uint16_big_endian(&stream);

    if (result.units_per_em < 64 || result.units_per_em > 16384)
    {
        RETURN_ZEROED(result);
    }

    result.created = memory_stream_read_int64_big_endian(&stream);
    result.modified = memory_stream_read_int64_big_endian(&stream);
    result.x_min = memory_stream_read_int16_big_endian(&stream);
    result.y_min = memory_stream_read_int16_big_endian(&stream);
    result.x_max = memory_stream_read_int16_big_endian(&stream);
    result.y_max = memory_stream_read_int16_big_endian(&stream);
    result.mac_style = memory_stream_read_uint16_big_endian(&stream);
    result.lowest_rec_ppem = memory_stream_read_uint16_big_endian(&stream);
    result.font_direction_hint = memory_stream_read_int16_big_endian(&stream);
    result.index_to_loc_format = memory_stream_read_int16_big_endian(&stream);
    result.glyph_data_format = memory_stream_read_int16_big_endian(&stream);

    return result;
}

true_type_maximum_profile_table true_type_maximum_profile_table_parse(const true_type_table_directory_entry *maximum_profile_table_entry)
{
    true_type_maximum_profile_table result = {0};

    if (!maximum_profile_table_entry || !maximum_profile_table_entry->memory)
    {
        return result;
    }

    const usize true_type_maximum_profile_table_version05_length = 6;
    const usize true_type_maximum_profile_table_version10_length = 32;

    if (maximum_profile_table_entry->length < true_type_maximum_profile_table_version05_length)
    {
        return result;
    }

    memory_stream stream;
    memory_stream_initialize_read_only(&stream, maximum_profile_table_entry->memory, maximum_profile_table_entry->length);

    result.version = memory_stream_read_uint32_big_endian(&stream);
    result.num_glyphs = memory_stream_read_uint16_big_endian(&stream);

    const u32 true_type_maximum_profile_version05 = 0x00005000;
    const u32 true_type_maximum_profile_version10 = 0x00010000;

    if (result.version == true_type_maximum_profile_version05)
    {
        return result;
    }
    else if (result.version == true_type_maximum_profile_version10)
    {
        if (maximum_profile_table_entry->length < true_type_maximum_profile_table_version10_length)
        {
            RETURN_ZEROED(result);
        }

        result.max_points = memory_stream_read_uint16_big_endian(&stream);
        result.max_contours = memory_stream_read_uint16_big_endian(&stream);
        result.max_component_points = memory_stream_read_uint16_big_endian(&stream);
        result.max_component_contours = memory_stream_read_uint16_big_endian(&stream);
        result.max_zones = memory_stream_read_uint16_big_endian(&stream);
        result.max_twilight_points = memory_stream_read_uint16_big_endian(&stream);
        result.max_storage = memory_stream_read_uint16_big_endian(&stream);
        result.max_function_defs = memory_stream_read_uint16_big_endian(&stream);
        result.max_instruction_defs = memory_stream_read_uint16_big_endian(&stream);
        result.max_stack_elements = memory_stream_read_uint16_big_endian(&stream);
        result.max_size_of_instructions = memory_stream_read_uint16_big_endian(&stream);
        result.max_component_elements = memory_stream_read_uint16_big_endian(&stream);
        result.max_component_depth = memory_stream_read_uint16_big_endian(&stream);
    }
    else
    {
        RETURN_ZEROED(result);
    }

    return result;
}

true_type_cmap_format4 true_type_cmap_table_parse_format4(const true_type_table_directory_entry *cmap_table_entry)
{
    true_type_cmap_format4 result = {0};

    if (!cmap_table_entry || !cmap_table_entry->memory)
    {
        return result;
    }

    const usize true_type_cmap_header_length = 4;
    if (cmap_table_entry->length < true_type_cmap_header_length)
    {
        return result;
    }

    memory_stream stream;
    memory_stream_initialize_read_only(&stream, cmap_table_entry->memory, cmap_table_entry->length);

    u16 version = memory_stream_read_uint16_big_endian(&stream);
    if (version != 0)
    {
        return result;
    }

    u16 number_subtables = memory_stream_read_uint16_big_endian(&stream);

    const usize true_type_cmap_encoding_record_length = 8;
    if (cmap_table_entry->length < true_type_cmap_header_length + (number_subtables * true_type_cmap_encoding_record_length))
    {
        return result;
    }

    u32 selected_subtable_offset = 0;

    for (u16 subtable_index = 0; subtable_index < number_subtables; subtable_index++)
    {
        u16 platform_id = memory_stream_read_uint16_big_endian(&stream);
        u16 platform_specific_id = memory_stream_read_uint16_big_endian(&stream);
        u32 subtable_offset = memory_stream_read_uint32_big_endian(&stream);

        if (subtable_offset > cmap_table_entry->length || subtable_offset + 2 > cmap_table_entry->length)
        {
            continue;
        }

        bool is_windows_bmp = (platform_id == 3 && platform_specific_id == 1);
        bool is_unicode_bmp = (platform_id == 0 && (platform_specific_id == 3 || platform_specific_id == 4));

        if (is_windows_bmp || is_unicode_bmp)
        {
            u16 subtable_format = read_uint16_big_endian(&stream.memory[subtable_offset]);
            if (subtable_format == 4)
            {
                selected_subtable_offset = subtable_offset;
                break;
            }
        }
    }

    if (selected_subtable_offset == 0)
    {
        return result;
    }

    const u8 *subtable_pointer = &stream.memory[selected_subtable_offset];
    usize subtable_remaining_length = cmap_table_entry->length - selected_subtable_offset;

    const usize true_type_cmap_format4_header_length = 14;
    if (subtable_remaining_length < true_type_cmap_format4_header_length)
    {
        return result;
    }

    u16 format = read_uint16_big_endian(&subtable_pointer[0]);
    if (format != 4)
    {
        return result;
    }

    u16 length = read_uint16_big_endian(&subtable_pointer[2]);
    u16 seg_count_x2 = read_uint16_big_endian(&subtable_pointer[6]);

    u16 seg_count = seg_count_x2 / 2;

    usize expected_subtable_minimum_length = true_type_cmap_format4_header_length + (seg_count * 8) + 2;

    if (length < expected_subtable_minimum_length || length > subtable_remaining_length)
    {
        return result;
    }

    result.is_valid = true;
    result.seg_count = seg_count;
    result.memory = subtable_pointer;
    result.length = length;

    result.end_code_offset = true_type_cmap_format4_header_length;
    result.start_code_offset = result.end_code_offset + seg_count_x2 + 2;
    result.id_delta_offset = result.start_code_offset + seg_count_x2;
    result.id_range_offset_offset = result.id_delta_offset + seg_count_x2;

    return result;
}

u32 true_type_cmap_format4_get_glyph_index(const true_type_cmap_format4 *cmap, u32 character_code)
{
    if (!cmap || !cmap->is_valid || !cmap->memory || character_code > 0xFFFF)
    {
        return 0;
    }

    const u8 *subtable_pointer = cmap->memory;
    u16 target_character_code = (u16)character_code;

    u16 segment_index = 0;
    bool segment_found = false;

    for (u16 current_index = 0; current_index < cmap->seg_count; current_index++)
    {
        u16 end_code = read_uint16_big_endian(&subtable_pointer[cmap->end_code_offset + (current_index * 2)]);
        if (end_code >= target_character_code)
        {
            segment_index = current_index;
            segment_found = true;
            break;
        }
    }

    if (!segment_found)
    {
        return 0;
    }

    u16 start_code = read_uint16_big_endian(&subtable_pointer[cmap->start_code_offset + (segment_index * 2)]);
    if (start_code > target_character_code)
    {
        // NOTE: the character code is greater than the previous segment's end_code
        // but less than this segment's start_code, so the poor guy has no mapping
        // :(
        return 0;
    }

    i16 id_delta = read_int16_big_endian(&subtable_pointer[cmap->id_delta_offset + (segment_index * 2)]);
    u16 id_range_offset = read_uint16_big_endian(&subtable_pointer[cmap->id_range_offset_offset + (segment_index * 2)]);

    if (id_range_offset == 0)
    {
        // NOTE: if id_range_offset is 0, the mapping adds id_delta to the character
        // code
        return (u32)((target_character_code + id_delta) & 0xFFFF);
    }
    else
    {
        // NOTE: if id_range_offset is non-zero, it relies on the glyph_index_array
        // glyph_index_address = id_range_offset[i] + 2 * (c - start_code[i]) +
        // (ptr) &id_range_offset[i]

        usize id_range_offset_location = cmap->id_range_offset_offset + (segment_index * 2);
        usize offset_to_glyph_index = id_range_offset_location + id_range_offset + (2 * (target_character_code - start_code));

        if (offset_to_glyph_index + 2 > cmap->length)
        {
            return 0;
        }

        u16 glyph_index = read_uint16_big_endian(&subtable_pointer[offset_to_glyph_index]);
        if (glyph_index != 0)
        {
            return (u32)((glyph_index + id_delta) & 0xFFFF);
        }
        else
        {
            return 0;
        }
    }
}
true_type_index_to_location_table true_type_index_to_location_table_parse(const true_type_table_directory_entry *index_to_location_table_entry, i16 index_to_loc_format, u16 num_glyphs)
{
    true_type_index_to_location_table result = {0};

    if (!index_to_location_table_entry || !index_to_location_table_entry->memory)
    {
        return result;
    }
    if (index_to_loc_format != 0 && index_to_loc_format != 1)
    {
        return result;
    }

    usize expected_entry_count = (usize)num_glyphs + 1;
    usize expected_minimum_length = 0;

    if (index_to_loc_format == 0)
    {
        expected_minimum_length = expected_entry_count * 2;
    }
    else
    {
        expected_minimum_length = expected_entry_count * 4;
    }

    if (index_to_location_table_entry->length < expected_minimum_length)
    {
        return result;
    }

    result.is_valid = true;
    result.index_to_loc_format = index_to_loc_format;
    result.num_glyphs = num_glyphs;
    result.memory = index_to_location_table_entry->memory;
    result.length = index_to_location_table_entry->length;

    return result;
}

true_type_glyph_location true_type_index_to_location_get_glyph_location(const true_type_index_to_location_table *loca, u16 glyph_index)
{
    true_type_glyph_location result = {0};

    if (!loca || !loca->is_valid || !loca->memory)
    {
        return result;
    }

    if (glyph_index >= loca->num_glyphs)
    {
        return result;
    }

    u32 current_glyph_offset = 0;
    u32 next_glyph_offset = 0;

    if (loca->index_to_loc_format == 0)
    {
        usize byte_offset = (usize)glyph_index * 2;

        u16 raw_current_offset = read_uint16_big_endian(&loca->memory[byte_offset]);
        u16 raw_next_offset = read_uint16_big_endian(&loca->memory[byte_offset + 2]);

        current_glyph_offset = (u32)raw_current_offset * 2;
        next_glyph_offset = (u32)raw_next_offset * 2;
    }
    else
    {
        usize byte_offset = (usize)glyph_index * 4;

        current_glyph_offset = read_uint32_big_endian(&loca->memory[byte_offset]);
        next_glyph_offset = read_uint32_big_endian(&loca->memory[byte_offset + 4]);
    }

    if (next_glyph_offset >= current_glyph_offset)
    {
        result.offset = current_glyph_offset;
        result.length = next_glyph_offset - current_glyph_offset;
    }

    return result;
}

true_type_simple_glyph true_type_glyf_table_parse_simple_glyph(memory_arena *arena, memory_stream *error_stream, const true_type_table_directory_entry *glyf_table_entry, true_type_glyph_location glyph_location)
{
    true_type_simple_glyph result = {0};

    if (!glyf_table_entry || !glyf_table_entry->memory)
    {
        memory_stream_write_string(error_stream, "error: invalid parameter: glyf_table_entry.");

        return result;
    }

    if (glyph_location.offset > glyf_table_entry->length || glyph_location.length > (glyf_table_entry->length - glyph_location.offset))
    {
        memory_stream_write_string(error_stream, "error: glyph location out of bounds in glyf table.");

        return result;
    }

    if (glyph_location.length == 0)
    {
        result.is_valid = true;
        return result;
    }

    memory_stream stream;
    memory_stream_initialize_read_only(&stream, &glyf_table_entry->memory[glyph_location.offset], glyph_location.length);

    const usize true_type_glyph_header_length = 10;
    if (glyph_location.length < true_type_glyph_header_length)
    {
        memory_stream_write_string(error_stream, "error: glyph data too small to contain header.");

        return result;
    }

    result.number_of_contours = memory_stream_read_int16_big_endian(&stream);

    result.x_min = memory_stream_read_int16_big_endian(&stream);
    result.y_min = memory_stream_read_int16_big_endian(&stream);
    result.x_max = memory_stream_read_int16_big_endian(&stream);
    result.y_max = memory_stream_read_int16_big_endian(&stream);

    if (result.number_of_contours < 0)
    {
        result.is_valid = true;
        result.is_compound = true;

        return result;
    }
    else if (result.number_of_contours == 0)
    {
        result.is_valid = true;

        return result;
    }

    usize end_points_array_byte_length = (usize)result.number_of_contours * 2;
    if (stream.offset + end_points_array_byte_length > glyph_location.length)
    {
        memory_stream_write_string(error_stream, "error: end points array overruns glyph data.");

        return result;
    }

    u16 last_point_index = read_uint16_big_endian(&stream.memory[stream.offset + end_points_array_byte_length - 2]);
    result.number_of_points = (u32)last_point_index + 1;

    result.end_points_of_contours = MEMORY_ARENA_PUSH_ARRAY(arena, u16, result.number_of_contours);
    result.points = MEMORY_ARENA_PUSH_ARRAY(arena, true_type_glyph_point, result.number_of_points);

    if (!result.end_points_of_contours || !result.points)
    {
        memory_stream_write_string(error_stream, "error: out of memory.");

        return result;
    }

    for (i16 contour_index = 0; contour_index < result.number_of_contours; contour_index++)
    {
        result.end_points_of_contours[contour_index] = memory_stream_read_uint16_big_endian(&stream);
    }

    if (stream.offset + 2 > glyph_location.length)
    {
        memory_stream_write_string(error_stream, "error: instruction length field overruns glyph data.");
        RETURN_ZEROED(result);
    }

    result.instruction_length = memory_stream_read_uint16_big_endian(&stream);

    if (stream.offset + result.instruction_length > glyph_location.length)
    {
        memory_stream_write_string(error_stream, "error: instructions overrun glyph data.");
        RETURN_ZEROED(result);
    }

    result.instructions = &stream.memory[stream.offset];
    stream.offset += result.instruction_length;

    u8 *unpacked_flags = MEMORY_ARENA_PUSH_ARRAY(arena, u8, result.number_of_points);
    if (!unpacked_flags)
    {
        memory_stream_write_string(error_stream, "error: out of memory.");
        RETURN_ZEROED(result);
    }

    u32 points_processed = 0;
    while (points_processed < result.number_of_points)
    {
        if (stream.offset + 1 > glyph_location.length)
        {
            memory_stream_write_string(error_stream, "error: flags overrun glyph data.");
            RETURN_ZEROED(result);
        }

        u8 current_flag = stream.memory[stream.offset++];
        unpacked_flags[points_processed++] = current_flag;

        if (IS_BIT_SET(current_flag, true_type_glyph_flag_repeat))
        {
            if (stream.offset + 1 > glyph_location.length)
            {
                memory_stream_write_string(error_stream, "error: flag repeat count overruns glyph data.");
                RETURN_ZEROED(result);
            }

            u8 repeat_count = stream.memory[stream.offset++];

            if (points_processed + repeat_count > result.number_of_points)
            {
                memory_stream_write_string(error_stream, "error: flag repeat count exceeds point count.");
                RETURN_ZEROED(result);
            }

            for (u8 repeat_index = 0; repeat_index < repeat_count; repeat_index++)
            {
                unpacked_flags[points_processed++] = current_flag;
            }
        }
    }

    i16 current_x = 0;
    for (u32 point_index = 0; point_index < result.number_of_points; point_index++)
    {
        u8 flag = unpacked_flags[point_index];
        result.points[point_index].is_on_curve = IS_BIT_SET(flag, true_type_glyph_flag_on_curve);

        if (IS_BIT_SET(flag, true_type_glyph_flag_x_short_vector))
        {
            if (stream.offset + 1 > glyph_location.length)
            {
                memory_stream_write_string(error_stream, "error: X coordinate byte overruns glyph data.");
                RETURN_ZEROED(result);
            }

            u8 delta_x = stream.memory[stream.offset++];
            if (IS_BIT_SET(flag, true_type_glyph_flag_x_is_same_or_positive_x_short))
            {
                current_x += delta_x;
            }
            else
            {
                current_x -= delta_x;
            }
        }
        else
        {
            if (!IS_BIT_SET(flag, true_type_glyph_flag_x_is_same_or_positive_x_short))
            {
                if (stream.offset + 2 > glyph_location.length)
                {
                    memory_stream_write_string(error_stream, "error: Y coordinate byte overruns glyph data.");
                    RETURN_ZEROED(result);
                }

                i16 delta_x = memory_stream_read_int16_big_endian(&stream);
                current_x += delta_x;
            }
        }
        result.points[point_index].x = current_x;
    }

    i16 current_y = 0;
    for (u32 point_index = 0; point_index < result.number_of_points; point_index++)
    {
        u8 flag = unpacked_flags[point_index];

        if (IS_BIT_SET(flag, true_type_glyph_flag_y_short_vector))
        {
            if (stream.offset + 1 > glyph_location.length)
            {
                memory_stream_write_string(error_stream, "error: Y coordinate byte overruns glyph data.");
                RETURN_ZEROED(result);
            }

            u8 delta_y = stream.memory[stream.offset++];
            if (IS_BIT_SET(flag, true_type_glyph_flag_y_is_same_or_positive_y_short))
            {
                current_y += delta_y;
            }
            else
            {
                current_y -= delta_y;
            }
        }
        else
        {
            if (!IS_BIT_SET(flag, true_type_glyph_flag_y_is_same_or_positive_y_short))
            {
                if (stream.offset + 2 > glyph_location.length)
                {
                    memory_stream_write_string(error_stream, "error: Y coordinate short overruns glyph data.");
                    RETURN_ZEROED(result);
                }

                i16 delta_y = memory_stream_read_int16_big_endian(&stream);
                current_y += delta_y;
            }
        }
        result.points[point_index].y = current_y;
    }

    result.is_valid = true;
    return result;
}

// NOTE: rasterization. I stole and adapted it from here:
// https://handmade.network/forums/wip/t/7610-reading_ttf_files_and_rasterizing_them_using_a_handmade_approach,_part_2__rasterization
// love you twin thanks!!!

static void true_type_tessellate_bezier(vector2 *output_points, u32 *output_size, vector2 p0, vector2 p1, vector2 p2)
{
    const u32 subdivision_count = 3;
    f32 step = 1.0f / (f32)subdivision_count;

    for (u32 i = 1; i <= subdivision_count; i++)
    {
        f32 t = (f32)i * step;
        f32 t1 = 1.0f - t;
        f32 t2 = t * t;

        f32 x = (t1 * t1 * p0.x) + (2.0f * t1 * t * p1.x) + (t2 * p2.x);
        f32 y = (t1 * t1 * p0.y) + (2.0f * t1 * t * p1.y) + (t2 * p2.y);

        output_points[*output_size] = v2(x, y);
        (*output_size)++;
    }
}

image true_type_glyph_rasterize(memory_arena *arena, memory_stream *error_stream, const true_type_simple_glyph *glyph, f32 scale)
{
    image result = {0};

    if (!glyph || !glyph->is_valid || glyph->is_compound || glyph->number_of_contours <= 0 || scale <= 0.0f)
    {
        return result;
    }

    if (glyph->y_max <= glyph->y_min || glyph->x_max <= glyph->x_min)
    {
        memory_stream_write_string(error_stream, "error: weird glyph bounds.");

        return result;
    }

    u32 target_pixel_width = (u32)(((f32)(glyph->x_max - glyph->x_min) * scale) + 0.999f);
    u32 target_pixel_height = (u32)(((f32)(glyph->y_max - glyph->y_min) * scale) + 0.999f);
    if (target_pixel_width == 0)
    {
        target_pixel_width = 1;
    }

    if (target_pixel_height == 0)
    {
        target_pixel_height = 1;
    }

    result.size.x = target_pixel_width;
    result.size.y = target_pixel_height;
    result.bytes_per_pixel = 4;

    usize pixels_allocation_size = (usize)result.size.x * (usize)result.size.y * result.bytes_per_pixel;
    result.pixels = MEMORY_ARENA_PUSH_BYTES(arena, pixels_allocation_size);

    if (!result.pixels)
    {
        memory_stream_write_string(error_stream, "error: out of memory.");

        return result;
    }

    u32 max_generated_points = glyph->number_of_points * 10;

    vector2 *generated_points = MEMORY_ARENA_PUSH_ARRAY(arena, vector2, max_generated_points);
    if (!generated_points)
    {
        memory_stream_write_string(error_stream, "error: out of memory.");

        return result;
    }

    u32 *contour_end_indices = MEMORY_ARENA_PUSH_ARRAY(arena, u32, glyph->number_of_contours);
    if (!contour_end_indices)
    {
        memory_stream_write_string(error_stream, "error: out of memory.");

        return result;
    }

    u32 generated_point_count = 0;
    u32 current_point_index = 0;

    for (i16 contour_index = 0; contour_index < glyph->number_of_contours; contour_index++)
    {
        u32 contour_start_index = current_point_index;
        u32 contour_end_index = glyph->end_points_of_contours[contour_index];
        u32 contour_length = contour_end_index - contour_start_index + 1;

        vector2 start_anchor;
        bool first_is_on_curve = glyph->points[contour_start_index].is_on_curve;
        bool last_is_on_curve = glyph->points[contour_end_index].is_on_curve;

        if (first_is_on_curve)
        {
            start_anchor = v2((f32)glyph->points[contour_start_index].x, (f32)glyph->points[contour_start_index].y);
        }
        else if (last_is_on_curve)
        {
            start_anchor = v2((f32)glyph->points[contour_end_index].x, (f32)glyph->points[contour_end_index].y);
        }
        else
        {
            start_anchor = v2((f32)(glyph->points[contour_start_index].x + glyph->points[contour_end_index].x) / 2.0f, (f32)(glyph->points[contour_start_index].y + glyph->points[contour_end_index].y) / 2.0f);
        }

        generated_points[generated_point_count++] = start_anchor;

        for (u32 i = 0; i <= contour_length; i++)
        {
            u32 current_index = contour_start_index + (i % contour_length);
            u32 next_index = contour_start_index + ((i + 1) % contour_length);

            bool current_is_on_curve = glyph->points[current_index].is_on_curve;
            bool next_is_on_curve = glyph->points[next_index].is_on_curve;

            vector2 point_current = v2((f32)glyph->points[current_index].x, (f32)glyph->points[current_index].y);
            vector2 point_next = v2((f32)glyph->points[next_index].x, (f32)glyph->points[next_index].y);

            if (current_is_on_curve)
            {
                generated_points[generated_point_count++] = point_current;
            }
            else
            {
                vector2 p0 = generated_points[generated_point_count - 1];
                vector2 p1 = point_current;
                vector2 p2;

                if (next_is_on_curve)
                {
                    p2 = point_next;
                }
                else
                {
                    p2 = v2((point_current.x + point_next.x) / 2.0f, (point_current.y + point_next.y) / 2.0f);
                }

                u32 added_points = 0;
                true_type_tessellate_bezier(generated_points + generated_point_count, &added_points, p0, p1, p2);
                generated_point_count += added_points;
            }
        }

        generated_points[generated_point_count++] = start_anchor;

        contour_end_indices[contour_index] = generated_point_count;
        current_point_index = contour_end_index + 1;
    }

    for (u32 i = 0; i < generated_point_count; i++)
    {
        generated_points[i].x = (generated_points[i].x - (f32)glyph->x_min) * scale;
        generated_points[i].y = ((f32)glyph->y_max - generated_points[i].y) * scale;
    }

    true_type_edge *edges = MEMORY_ARENA_PUSH_ARRAY(arena, true_type_edge, generated_point_count);
    u32 edge_count = 0;
    u32 read_point_index = 0;

    for (i16 contour_index = 0; contour_index < glyph->number_of_contours; contour_index++)
    {
        for (; read_point_index < contour_end_indices[contour_index] - 1; read_point_index++)
        {
            edges[edge_count].p1 = generated_points[read_point_index];
            edges[edge_count].p2 = generated_points[read_point_index + 1];
            edge_count++;
        }

        read_point_index++;
    }

    u8 *coverage_buffer = MEMORY_ARENA_PUSH_BYTES(arena, result.size.x * result.size.y);
    if (!coverage_buffer)
    {
        memory_stream_write_string(error_stream, "error: out of memory.");
        RETURN_ZEROED(result);
    }

    const u32 scanline_subdivisions = 5;
    f32 alpha_weight = 255.0f / (f32)scanline_subdivisions;
    f32 step_per_scanline = 1.0f / (f32)scanline_subdivisions;

    const u32 max_intersections = 256;
    true_type_scanline_intersection intersections[256];

    for (u32 y = 0; y < result.size.y; y++)
    {
        for (u32 sub_y = 0; sub_y < scanline_subdivisions; sub_y++)
        {
            u32 intersection_count = 0;
            f32 scanline = (f32)y + (f32)sub_y * step_per_scanline;

            for (u32 e = 0; e < edge_count; e++)
            {
                true_type_edge *edge = &edges[e];

                f32 bigger_y = MAX(edge->p1.y, edge->p2.y);
                f32 smaller_y = MIN(edge->p1.y, edge->p2.y);

                if (scanline <= smaller_y || scanline > bigger_y)
                {
                    continue;
                }

                f32 delta_x = edge->p2.x - edge->p1.x;
                f32 delta_y = edge->p2.y - edge->p1.y;

                if (delta_y == 0.0f)
                {
                    continue;
                }

                i32 direction = (delta_y > 0.0f) ? 1 : -1;

                f32 intersection_x = 0.0f;
                if (delta_x == 0.0f)
                {
                    intersection_x = edge->p1.x;
                }
                else
                {
                    intersection_x = (scanline - edge->p1.y) * (delta_x / delta_y) + edge->p1.x;

                    if (intersection_x < 0.0f)
                    {
                        intersection_x = edge->p1.x;
                    }
                }

                if (intersection_count < max_intersections)
                {
                    intersections[intersection_count].x = intersection_x;
                    intersections[intersection_count].direction = direction;
                    intersection_count++;
                }
            }

            for (u32 i = 1; i < intersection_count; i++)
            {
                true_type_scanline_intersection key = intersections[i];
                i32 j = (i32)i - 1;

                while (j >= 0 && intersections[j].x > key.x)
                {
                    intersections[j + 1] = intersections[j];
                    j--;
                }

                intersections[j + 1] = key;
            }

            i32 winding_number = 0;
            f32 start_x = 0.0f;

            for (u32 m = 0; m < intersection_count; m++)
            {
                i32 next_winding_number = winding_number + intersections[m].direction;

                if (winding_number == 0 && next_winding_number != 0)
                {
                    start_x = intersections[m].x;
                }
                else if (winding_number != 0 && next_winding_number == 0)
                {
                    f32 end_x = intersections[m].x;

                    if (end_x > start_x)
                    {
                        i32 start_index = (i32)start_x;
                        f32 start_covered = ((f32)(start_index + 1)) - start_x;

                        i32 end_index = (i32)end_x;
                        f32 end_covered = end_x - (f32)end_index;

                        if (start_index < 0)
                        {
                            start_index = 0;
                            start_covered = 1.0f;
                        }
                        if (end_index >= (i32)result.size.x)
                        {
                            end_index = result.size.x - 1;
                            end_covered = 1.0f;
                        }

                        if (start_index < (i32)result.size.x && end_index >= 0)
                        {
                            usize buffer_index_y = (usize)y * result.size.x;

                            if (start_index == end_index)
                            {
                                f32 value = (f32)coverage_buffer[start_index + buffer_index_y] + (alpha_weight * (start_covered + end_covered - 1.0f));
                                coverage_buffer[start_index + buffer_index_y] = (u8)MIN(value, 255.0f);
                            }
                            else
                            {
                                f32 value_start = (f32)coverage_buffer[start_index + buffer_index_y] + (alpha_weight * start_covered);
                                coverage_buffer[start_index + buffer_index_y] = (u8)MIN(value_start, 255.0f);

                                f32 value_end = (f32)coverage_buffer[end_index + buffer_index_y] + (alpha_weight * end_covered);
                                coverage_buffer[end_index + buffer_index_y] = (u8)MIN(value_end, 255.0f);

                                for (i32 j = start_index + 1; j < end_index; j++)
                                {
                                    f32 val = (f32)coverage_buffer[j + buffer_index_y] + alpha_weight;
                                    coverage_buffer[j + buffer_index_y] = (u8)MIN(val, 255.0f);
                                }
                            }
                        }
                    }
                }

                winding_number = next_winding_number;
            }
        }
    }

    for (u32 i = 0; i < result.size.x * result.size.y; i++)
    {
        u8 alpha = coverage_buffer[i];

        result.pixels[i * 4 + 0] = 255;
        result.pixels[i * 4 + 1] = 255;
        result.pixels[i * 4 + 2] = 255;
        result.pixels[i * 4 + 3] = alpha;
    }

    return result;
}

image true_type_font_bake_atlas(memory_arena *permanent_arena, memory_arena *temporary_arena, memory_stream *error_stream, const true_type_font *font, u32 target_pixel_height, u32 atlas_width, u32 atlas_height, u32 first_character, u32 character_count, true_type_baked_glyph *out_glyphs)
{
    image result = {0};

    if (!font || atlas_width == 0 || atlas_height == 0 || character_count == 0 || !out_glyphs)
    {
        return result;
    }

    ZERO_ITEMS(out_glyphs, character_count);

    const true_type_table_directory_entry *head_entry = true_type_font_get_table(font, true_type_table_tag_head);
    const true_type_table_directory_entry *maxp_entry = true_type_font_get_table(font, true_type_table_tag_maxp);
    const true_type_table_directory_entry *cmap_entry = true_type_font_get_table(font, true_type_table_tag_cmap);
    const true_type_table_directory_entry *loca_entry = true_type_font_get_table(font, true_type_table_tag_loca);
    const true_type_table_directory_entry *glyf_entry = true_type_font_get_table(font, true_type_table_tag_glyf);

    if (!head_entry || !maxp_entry || !cmap_entry || !loca_entry || !glyf_entry)
    {
        memory_stream_write_string(error_stream, "error: could not get one or more tables.");

        return result;
    }

    true_type_head_table head = true_type_head_table_parse(head_entry);
    true_type_maximum_profile_table maxp = true_type_maximum_profile_table_parse(maxp_entry);
    true_type_cmap_format4 cmap = true_type_cmap_table_parse_format4(cmap_entry);
    true_type_index_to_location_table loca = true_type_index_to_location_table_parse(loca_entry, head.index_to_loc_format, maxp.num_glyphs);

    if (!cmap.is_valid || !loca.is_valid)
    {
        memory_stream_write_string(error_stream, "error: cmap/loca table is invalid.");

        return result;
    }

    f32 scale = (f32)target_pixel_height / (f32)head.units_per_em;

    result.size.x = atlas_width;
    result.size.y = atlas_height;
    result.bytes_per_pixel = 4;

    usize pixels_allocation_size = (usize)result.size.x * (usize)result.size.y * result.bytes_per_pixel;
    result.pixels = MEMORY_ARENA_PUSH_BYTES(permanent_arena, pixels_allocation_size);

    if (!result.pixels)
    {
        memory_stream_write_string(error_stream, "error: out of memory.");
        RETURN_ZEROED(result);
    }

    pack2d atlas_pack2d = pack2d_create(temporary_arena, atlas_width, atlas_height);
    const u32 padding = 1;

    for (u32 index = 0; index < character_count; index++)
    {
        memory_arena_checkpoint glyph_checkpoint = memory_arena_create_checkpoint(temporary_arena);

        u32 character_code = first_character + index;
        out_glyphs[index].character_code = character_code;

        u32 glyph_index = true_type_cmap_format4_get_glyph_index(&cmap, character_code);
        if (glyph_index == 0)
        {
            continue;
        }

        true_type_glyph_location glyph_location = true_type_index_to_location_get_glyph_location(&loca, (u16)glyph_index);
        true_type_simple_glyph glyph = true_type_glyf_table_parse_simple_glyph(temporary_arena, error_stream, glyf_entry, glyph_location);

        if (!glyph.is_valid)
        {
            memory_arena_restore_checkpoint(glyph_checkpoint);

            continue;
        }

        image image = true_type_glyph_rasterize(temporary_arena, error_stream, &glyph, scale);

        if (image.pixels && image.size.x > 0 && image.size.y > 0)
        {
            u32 position_x = 0;
            u32 position_y = 0;

            bool is_packed = pack2d_add(&atlas_pack2d, image.size.x + padding, image.size.y + padding, &position_x, &position_y);

            if (is_packed)
            {
                for (u32 y = 0; y < (u32)image.size.y; y++)
                {
                    for (u32 x = 0; x < (u32)image.size.x; x++)
                    {
                        u32 source_index = (y * (u32)image.size.x + x) * result.bytes_per_pixel;
                        u32 destination_index = ((position_y + y) * atlas_width + (position_x + x)) * result.bytes_per_pixel;

                        memory_copy_forwards(&result.pixels[destination_index], &image.pixels[source_index], 4);
                    }
                }

                out_glyphs[index].is_valid = true;
                out_glyphs[index].uv_min.x = (f32)position_x / (f32)atlas_width;
                out_glyphs[index].uv_min.y = (f32)position_y / (f32)atlas_height;
                out_glyphs[index].uv_max.x = (f32)(position_x + image.size.x) / (f32)atlas_width;
                out_glyphs[index].uv_max.y = (f32)(position_y + image.size.y) / (f32)atlas_height;
                out_glyphs[index].size.x = (f32)image.size.x;
                out_glyphs[index].size.y = (f32)image.size.y;
                out_glyphs[index].offset.x = (f32)glyph.x_min * scale;
                out_glyphs[index].offset.y = (f32)glyph.y_max * scale;
            }
        }

        memory_arena_restore_checkpoint(glyph_checkpoint);
    }

    return result;
}
