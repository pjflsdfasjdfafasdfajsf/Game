#include "game_platform.h"
#include "game_png.h"
#include "game_types.h"

UPDATE_AND_RENDER(update_and_render) {
    if (!memory->is_initialized) {
        static const char watermelon[] = {
#include "animation.png.h"
        };

        image image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, watermelon, sizeof(watermelon));
        render_allocate_texture(command_buffer, 1, image.size, image.bytes_per_pixel, image.pixels);

        memory->is_initialized = true;
    }

    const f32 size = 1.0f / 6.0f;
    const u32 row = 2;
    static u32 element = 0;
    element = (element + 1) % 6;

    render_clear_entire_screen(command_buffer, red);
    render_draw_rectangle(command_buffer, v2(10, 10), v2(200, 200), red, rect(element * size, row * size, element * size + size, row * size + size), 1);
}

GET_SOUND_SAMPLES(get_sound_samples) {
    UNUSED(sound_buffer);
}
