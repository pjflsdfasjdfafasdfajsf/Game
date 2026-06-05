// TODO:
// * fix hot-reloading
#include "game_platform.h"
#include "game_png.h"
#include "game_types.h"

UPDATE_AND_RENDER(update_and_render) {
    if (!memory->is_initialized) {
        static const char watermelon[] = {
#include "watermelon.png.h"
        };

        image image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, watermelon, sizeof(watermelon));
        render_allocate_texture(command_buffer, 1, image.size, image.bytes_per_pixel, image.pixels);

        memory->is_initialized = true;
    }

    render_clear_entire_screen(command_buffer, black);
    render_draw_rectangle(command_buffer, v2(10, 10), v2(200, 200), red, UNTEXTURED);
}

GET_SOUND_SAMPLES(get_sound_samples) {
    UNUSED(sound_buffer);
}
