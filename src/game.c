#include "game_platform.h"
#include "game_png.h"
#include "game_types.h"

UPDATE_AND_RENDER(update_and_render)
{
    vector2 *position = (vector2 *)memory->permanent_arena.base_pointer;

    if (!memory->is_initialized)
    {
        memory_arena_allocate_bytes(&memory->permanent_arena, sizeof(vector2));

        static const char watermelon[] = {
#embed "../assets/images/watermelon.png"
        };
        image image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, watermelon, sizeof(watermelon));
        render_allocate_texture(render_buffer, 1, image.size, image.bytes_per_pixel, image.pixels);

        *position = v2(10, 10);

        memory->is_initialized = true;
    }

    if (button_held(input->keys[key_code_w]))
    {
        position->y -= 500.0f * delta_time;
    }
    if (button_held(input->keys[key_code_s]))
    {
        position->y += 500.0f * delta_time;
    }
    if (button_held(input->keys[key_code_a]))
    {
        position->x -= 500.0f * delta_time;
    }
    if (button_held(input->keys[key_code_d]))
    {
        position->x += 500.0f * delta_time;
    }

    render_clear_entire_screen(render_buffer, black);
    render_draw_rectangle(render_buffer, *position, v2(200, 200), red, 1);
}

GET_SOUND_SAMPLES(get_sound_samples)
{
    UNUSED(sound_buffer);
}