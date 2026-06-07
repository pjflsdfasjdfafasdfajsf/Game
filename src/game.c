#include "game.h"
#include "game_math.h"
#include "game_platform.h"
#include "game_png.h"
#include "game_types.h"

UPDATE_AND_RENDER(update_and_render)
{
    game_state *state = (game_state *)memory->permanent_arena.base_pointer;
    if (!memory->is_initialized)
    {
        MEMORY_ARENA_PUSH_BYTES(&memory->permanent_arena, sizeof(game_state));

        static const char animation[] = {
#embed "../assets/images/watermelon.png"
        };
        image image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, animation, sizeof(animation));
        render_allocate_texture(render_buffer, 1, image.size, image.pixels);

        state->position = v2(10, 10);

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
        state->position.x -= 500.0f * delta_time;
    }
    if (button_held(input->keys[key_code_d]))
    {
        state->position.x += 500.0f * delta_time;
    }

    rectangle player = rect(state->position, v2(200.0f, 200.0f));
    rectangle wall = rect(v2(10.0f, 1000.0f), v2(500.0f, 500.0f));

    aabb_collision_result collision = aabb_collision(player, wall);
    if (collision.is_colliding)
    {
        state->position = vector2_add(state->position, collision.penetration_depth);
        player = rect(state->position, v2(200.0f, 200.0f));
    }

    render_clear_entire_screen(render_buffer, BLACK);
    render_draw_rectangle(render_buffer, wall, WHITE, UNIT, UNTEXTURED);
    render_draw_rectangle(render_buffer, player, WHITE, UNIT, 1);
}

GET_SOUND_SAMPLES(get_sound_samples)
{
    UNUSED(sound_buffer);
}