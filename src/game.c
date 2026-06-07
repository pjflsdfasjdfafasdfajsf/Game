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
#embed "../assets/images/animation.png"
        };
        image image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, watermelon, sizeof(watermelon));
        render_allocate_texture(render_buffer, 1, image.size, image.bytes_per_pixel, image.pixels);

        *position = v2(10, 10);

        memory->is_initialized = true;
    }

    static bool look_right = true;
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
        look_right = false;
        position->x -= 500.0f * delta_time;
    }
    if (button_held(input->keys[key_code_d]))
    {
        look_right = true;
        position->x += 500.0f * delta_time;
    }

    const f32 size = 1.0f / 6.0f;
    const u32 row = 1;
    static u32 element = 0;
    static f32 accumelated_time = 0.0f;

    accumelated_time += delta_time;
    if (accumelated_time > 0.1f) {
        accumelated_time = 0.0f;
        element = (element + 1) % 6;
    }
    rectangle uv = rect(element * size, row * size, element * size + size, row * size + size);
    if (!look_right) {
        f32 tmp = uv.min.x;
        uv.min.x = uv.max.x;
        uv.max.x = tmp;
    }

    render_clear_entire_screen(render_buffer, red);
    render_draw_rectangle(render_buffer, *position, v2(200, 200), red, uv, 1);
    render_draw_rectangle(render_buffer, v2(0, 700), v2(1000, 500), blue, uv, UNTEXTURED);
}

GET_SOUND_SAMPLES(get_sound_samples)
{
    UNUSED(sound_buffer);
}
