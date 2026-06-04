#include "utilities/init.h"
#include "utilities/file.h"
#include "utilities/ppm.h"

#include "game_ttf.h"

#if defined(FUZZING)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    utilities_initialize();

    true_type_font font = true_type_font_load_from_memory(&permanent_arena, &error_stream, data, size);

    if (font.number_of_tables > 0) {
        const u32 target_pixel_height = 32;
        const u32 atlas_width = 128;
        const u32 atlas_height = 128;
        const u32 character_count = 10;

        true_type_baked_glyph glyphs[10];

        image atlas = true_type_font_bake_atlas(&permanent_arena, &temporary_arena, &error_stream, &font, target_pixel_height, atlas_width, atlas_height, ' ', character_count, glyphs);
        UNUSED(atlas);
    }

    return 0;
}

#else ///////////////////////////////////////////////////////

int main(void) {
    utilities_initialize();

    usize file_size = 0;
    void *file_data = read_entire_file("data/font.ttf", &file_size);

    if (!file_data) {
        printf("test data missing\n");

        return 1;
    }

    true_type_font font = true_type_font_load_from_memory(&permanent_arena, &error_stream, file_data, file_size);

    if (font.number_of_tables > 0) {
        u32 atlas_width = 256;
        u32 atlas_height = 256;
        u32 target_height = 32;

        true_type_baked_glyph glyphs[TRUETYPE_CHARACTER_COUNT_FOR_ASCII];

        image atlas = true_type_font_bake_atlas(&permanent_arena, &temporary_arena, &error_stream, &font, target_height, atlas_width, atlas_height, TRUETYPE_FIRST_CHARACTER_FOR_ASCII, TRUETYPE_CHARACTER_COUNT_FOR_ASCII, glyphs);

        if (atlas.pixels) {
            if (ppm_write("ttf.ppm", atlas.size.x, atlas.size.y, atlas.bytes_per_pixel, atlas.pixels)) {
                printf("wrote ttf.ppm\n");
            } else {
                printf("could not write ttf.ppm\n");
            }
        } else {
            printf("rasterization failed\n");
            printf("error steam: %.*s\n", (int)error_stream.offset, error_stream.memory);
        }
    } else {
        printf("table parsing failed\n");
        printf("error stream: %.*s\n", (int)error_stream.offset, error_stream.memory);
    }

    return 0;
}

#endif
