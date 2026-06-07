#pragma once

#include "game_platform.h"
#include "game_png.h"
#include "game_types.h"

#define TRUETYPE_FIRST_CHARACTER_FOR_ASCII 32
#define TRUETYPE_CHARACTER_COUNT_FOR_ASCII 95

typedef enum
{
    true_type_scaler_type_unknown = 0,
    true_type_scaler_type_windows,
    true_type_scaler_type_mac,
    true_type_scaler_type_post_script,
    true_type_scaler_type_open_type
} true_type_scaler_type;

typedef struct
{
    u32 tag;
    u32 check_sum;
    u32 offset;
    u32 length;
    const u8 *memory;
} true_type_table_directory_entry;

typedef struct
{
    true_type_scaler_type scaler_type;
    u16 number_of_tables;
    u16 search_range;
    u16 entry_selector;
    u16 range_shift;

    true_type_table_directory_entry *tables;
} true_type_font;

typedef struct
{
    u32 character_code;
    bool is_valid;
    vector2 uv_min;
    vector2 uv_max;
    vector2 size;
    vector2 offset;
} true_type_baked_glyph;

true_type_font true_type_font_load_from_memory(memory_arena *arena, memory_stream *error_stream, const void *memory, usize length);

image true_type_font_bake_atlas(memory_arena *permanent_arena, memory_arena *temporary_arena, memory_stream *error_stream, const true_type_font *font, u32 target_pixel_height, u32 atlas_width, u32 atlas_height, u32 first_character, u32 character_count, true_type_baked_glyph *out_glyphs);
