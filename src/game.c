#include "game.h"
#include "game_math.h"
#include "game_platform.h"
#include "game_png.h"
#include "game_map.h"
#include "game_types.h"

// NOTE: in seconds
const f32 enemy_hit_cooldown = 1.0f;

static void enemy_add(game_state *state, vector2 position, vector2 size) {
    if (state->enemy_count + 1 > ARRAY_COUNT(state->enemies)) {
        return;
    }

    state->enemies[state->enemy_count++] = (enemy){
        .position = position,
        .size = size, 
    };
}

UPDATE_AND_RENDER(update_and_render)
{
    game_state *state = (game_state *)memory->permanent_arena.base_pointer;
    if (!memory->is_initialized)
    {
        MEMORY_ARENA_PUSH_BYTES(&memory->permanent_arena, sizeof(game_state));

        static const char zig[] = {
#embed "../assets/images/zig.png"
        };
        image image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, zig, sizeof(zig));
        render_allocate_texture(render_buffer, 1, image.size, image.pixels);

        static const char gangster[] = {
#embed "../assets/images/gangster.png"  
        };

        map map = map_create("test.map");

        map_add(&map, rect(v2(0, 0), v2(10, 10)));
        map_add(&map, rect(v2(67, 68), v2(69, 70)));
        map_add(&map, rect(v2(100.0f, 101.0f), v2(102.f, 103.f)));

        map_write(memory, platform, &map);
        map_load(memory, platform, "test.map", &map);

        image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, gangster, sizeof(gangster)); 
        render_allocate_texture(render_buffer, 2, image.size, image.pixels);

        state->position = v2(10, 10);
        state->health = 100.0f;
        state->last_hit = 0.0f;
        state->accumelated_time = 0.0;

        state->enemy_count = 0;

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

    if (button_pressed(input->mouse_buttons[mouse_button_left])) {
        enemy_add(state, input->mouse_position, v2(state->health, state->health));
    }

    rectangle player = rect(state->position, v2(200.0f, 200.0f));
    rectangle wall = rect(v2(50.0f, 1000.0f), v2(1000.0f, 20.0f));

    aabb_collision_result collision = aabb_collision(player, wall);
    if (collision.is_colliding)
    {
        state->position = vector2_add(state->position, collision.penetration_depth);
        player = rect(state->position, v2(200.0f, 200.0f));
    }

    bool hit = false;
    for (u32 i = 0; i < state->enemy_count; i++) {
        enemy *enemy = &state->enemies[i];

        vector2 to_player = vector2_sub(state->position, enemy->position);
        to_player = vector2_norm(to_player);

        enemy->position = vector2_add(enemy->position, vector2_scale(to_player, delta_time * 200.0f));

        rectangle enemy_bounds = rect(enemy->position, enemy->size);

        aabb_collision_result collision = aabb_collision(player, enemy_bounds);
        if (collision.is_colliding && state->accumelated_time - state->last_hit > enemy_hit_cooldown) {
            state->last_hit = state->accumelated_time;
            state->health -= 35.0f;
            hit = true;
        }

        collision = aabb_collision(enemy_bounds, wall);
        if (collision.is_colliding)
        {
            enemy->position = vector2_add(enemy->position, collision.penetration_depth);
        }
    }

    state->accumelated_time += delta_time;

    vector2 start = v2(state->position.x + 100.0f, state->position.y + 100.0f);
    vector2 direction = vector2_sub(input->mouse_position, start);
    raycast_result ray = ray_intersect_rectangle_infinite(start, direction, wall);

    render_draw_rectangle(render_buffer, wall, WHITE, UNIT, UNTEXTURED);

    for (u32 i = 0; i < state->enemy_count; i++) {
        enemy *enemy = &state->enemies[i];
        rectangle enemy_bounds = rect(enemy->position, enemy->size);
        render_draw_rectangle(render_buffer, enemy_bounds, WHITE, UNIT, 2);
    }
    
    render_draw_rectangle(render_buffer, player, GREEN, UNIT, UNTEXTURED);
    render_draw_rectangle(render_buffer, player, WHITE, UNIT, 1);

    if (state->health < 0.0f || hit) {
        render_draw_rectangle(render_buffer, rect(v2(0, 0), v2(10000, 10000)), RED, UNIT, UNTEXTURED);
    }
    if (ray.is_hitting)
    {
        render_draw_line(render_buffer, start, ray.hit_position, GREEN);
    }
    else
    {
        render_draw_line(render_buffer, start, vector2_add(start, vector2_scale(vector2_norm(direction), 3000.0f)), RED);
    }
}

GET_SOUND_SAMPLES(get_sound_samples)
{
    UNUSED(sound_buffer);
}
