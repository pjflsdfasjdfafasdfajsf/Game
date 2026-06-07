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
        render_allocate_texture(render_buffer, 1, image.size, image.bytes_per_pixel, image.pixels);

        state->position = v2(30, 10);
        state->velocity = v2(0, 0);
        state->accelaration = v2(0, 0);
        state->player_size = v2(200, 200);
        state->look_right = true;
        state->on_ground = false;

        memory->is_initialized = true;
    }

    const f32 speed = 500.0f;
    const f32 jump_strength = 1500.0f;
    vector2 delta = v2(0, 0);
    if (state->on_ground && button_pressed(input->keys[key_code_w]))
    {
        state->accelaration.y = -jump_strength;
    }
    if (button_held(input->keys[key_code_a]))
    {
        state->look_right = false;
        delta.x = -1;
    }
    if (button_held(input->keys[key_code_d]))
    {
        state->look_right = true;
        delta.x = 1;
    }
    delta = vector2_norm(delta);
    state->position = vector2_add(state->position, vector2_scale(delta, speed * delta_time));

    const vector2 gravity = v2(0.0f, 9.89665f);
    state->accelaration = vector2_add(state->accelaration, gravity);

    state->velocity = vector2_add(state->velocity, vector2_scale(state->accelaration, delta_time));
    state->position = vector2_add(state->position, state->velocity);

    state->accelaration = v2(0, 0);

    state->on_ground = false;
    if (state->position.y + state->player_size.height > 700.0f &&
        state->position.y + state->player_size.height < 700.0f + 500.0f && 
        state->position.x + state->player_size.width > 10.0f && state->position.x < 1010.0f) {
        state->velocity.y = 0.0f;
        state->position.y = 700.0f - state->player_size.height;
        state->on_ground = true;
    }

    state->velocity = vector2_scale(state->velocity, 0.99f);

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
