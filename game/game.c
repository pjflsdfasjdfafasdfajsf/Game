// TODO:
// * fix hot-reloading
#include "game_platform.h"
#include "game_png.h"
#include "game_types.h"

UPDATE_AND_RENDER(update_and_render) {
    if (!memory->is_initialized) {
        memory_stream_write_line(memory->standard_info_stream, "game says hi!");

        static const char watermelon[] = {
#include "watermelon.png.h"
        };

        image image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, watermelon, sizeof(watermelon));
        render_allocate_texture(command_buffer, 1, image.size, image.bytes_per_pixel, image.pixels);

        memory->is_initialized = true;
    }

    render_clear_entire_screen(command_buffer, black);
    render_draw_rectangle(command_buffer, V2(10, 10), V2(200, 200), red, UNTEXTURED);
}

GET_SOUND_SAMPLES(get_sound_samples) {
    UNUSED(audio_buffer);
}
