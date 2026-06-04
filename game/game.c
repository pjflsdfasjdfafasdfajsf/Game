// TODO:
// * WAV files parsing.
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

// commented out temporarily until we have WAV parser so this sine wave does not bless our ears. but it works.
GET_SOUND_SAMPLES(get_sound_samples) {
    UNUSED(audio_buffer);

    // // TODO: move this into game_state later!
    // static f32 phase = 0.0f;

    // f32 phase_increment = 440.0f / (f32)audio_buffer->samples_per_second;
    // f32 *sample_pointer = audio_buffer->samples;

    // for (u32 frame_index = 0; frame_index < audio_buffer->frame_count; frame_index++) {
    //     f32 sample_value = (phase < 0.5f) ? 0.05f : -0.05f;

    //     phase += phase_increment;
    //     if (phase > 1.0f) {
    //         phase -= 1.0f;
    //     }

    //     for (u32 channel_index = 0; channel_index < audio_buffer->channel_count; channel_index++) {
    //         *sample_pointer++ = sample_value;
    //     }
    // }
}
