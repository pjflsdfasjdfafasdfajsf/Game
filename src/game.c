#include "game_platform.h"
#include "game_png.h"
#include "game_math.h"

UPDATE_AND_RENDER(update_and_render)
{
    game_state *state = (game_state *)memory->permanent_arena.base_pointer;
    if (!memory->is_initialized)
    {
        MEMORY_ARENA_PUSH_BYTES(&memory->permanent_arena, sizeof(game_state));

        static const char animation[] = {
#embed "../assets/images/animation.png"
        };
        image image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, animation, sizeof(animation));
        render_allocate_texture(render_buffer, 1, image.size, image.bytes_per_pixel, image.pixels);

        state->position = v2(10, 10);
        state->look_right = true;

        memory->is_initialized = true;
    }

    if (button_held(input->keys[key_code_w]))
    {
        state->position.y -= 500.0f * delta_time;
    }
    if (button_held(input->keys[key_code_s]))
    {
        state->position.y += 500.0f * delta_time;
    }
    if (button_held(input->keys[key_code_a]))
    {
        state->look_right = false;
        state->position.x -= 500.0f * delta_time;
    }
    if (button_held(input->keys[key_code_d]))
    {
        state->look_right = true;
        state->position.x += 500.0f * delta_time;
    }

    const f32 size = 1.0f / 6.0f;
    const u32 row = 1;

    state->accumulated_time += delta_time;
    if (state->accumulated_time > 0.1f)
    {
        state->accumulated_time = 0.0f;
        state->element = (state->element + 1) % 6;
    }

    rectangle uv = rect(state->element * size, row * size, state->element * size + size, row * size + size);
    if (!state->look_right)
    {
        SWAP(uv.min.x, uv.max.x);
    }

    render_clear_entire_screen(render_buffer, red);
    render_draw_rectangle(render_buffer, state->position, v2(200, 200), red, uv, 1);
    render_draw_rectangle(render_buffer, v2(0, 700), v2(1000, 500), blue, uv, UNTEXTURED);
}

GET_SOUND_SAMPLES(get_sound_samples)
{
    UNUSED(sound_buffer);
}