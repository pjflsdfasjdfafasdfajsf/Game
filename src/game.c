#include "game.h"
#include "game_map.h"
#include "game_math.h"
#include "game_platform.h"
#include "game_png.h"
#include "game_types.h"

// NOTE: in seconds
const f32 enemy_hit_cooldown = 1.0f;

static void enemy_add(game_state *state, vector2 position, vector2 size)
{
    if (state->enemy_count + 1 > ARRAY_COUNT(state->enemies))
    {
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

        image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, gangster, sizeof(gangster));
        render_allocate_texture(render_buffer, 2, image.size, image.pixels);

        /* NOTE: watch out that you embed the correct map when changing the name */
        map map = map_create("test.map");

        map_add(&map, rect(v2(100, 100), v2(10, 100)), RED);
        map_add(&map, rect(v2(100, 100), v2(100, 10)), BLUE);
        map_add(&map, rect(v2(60, 60), v2(40, 40)), WHITE);
        map_add(&map, rect(v2(50, 1000), v2(1000, 20)), WHITE);

        map_write(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, platform, &map);

        /* NOTE: change path of the map to be in assets?
         * im not sure because its right now just for testing
         * so i will keep it here for now */
        static const char map_data[] = {
#embed "../test.map"
        };

        map_load(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, map_data, sizeof(map_data), &state->test_map);

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

    if (button_pressed(input->mouse_buttons[mouse_button_left]))
    {

        enemy_add(state, input->mouse_position, v2(state->health, state->health));
    }

    rectangle player = rect(state->position, v2(200.0f, 200.0f));

    /* NOTE: remove later only here for the ray visualisation on the map */
    bool ray_hit = false;
    vector2 ray_start;
    vector2 ray_end;
    vector2 ray_direction;

    /* NOTE: player collisions with the map */
    for (u32 i = 0; i < state->test_map.wall_count; i++)
    {
        map_wall *wall = &state->test_map.walls[i];

        aabb_collision_result collision = aabb_collision(player, wall->bounding_box);
        if (collision.is_colliding)
        {
            state->position = vector2_add(state->position, collision.penetration_depth);
            player = rect(state->position, v2(200.0f, 200.0f));
        }

        vector2 start = v2(state->position.x + 100.0f, state->position.y + 100.0f);
        vector2 direction = vector2_sub(input->mouse_position, start);
        raycast_result ray = ray_intersect_rectangle_infinite(start, direction, wall->bounding_box);

        /* NOTE: doesnt check the nearest collision at the moment so the ray might hit through another object */
        if (ray.is_hitting)
        {
            ray_hit = true;
            ray_end = ray.hit_position;
        }
        ray_direction = direction;
        ray_start = start;
    }

    /* NOTE: enemy collisions with the map */
    bool hit = false;
    for (u32 i = 0; i < state->enemy_count; i++)
    {
        enemy *enemy = &state->enemies[i];

        vector2 to_player = vector2_sub(state->position, enemy->position);
        to_player = vector2_norm(to_player);

        enemy->position = vector2_add(enemy->position, vector2_scale(to_player, delta_time * 200.0f));

        rectangle enemy_bounds = rect(enemy->position, enemy->size);

        /* NOTE: collision with player */
        aabb_collision_result collision = aabb_collision(player, enemy_bounds);
        if (collision.is_colliding && state->accumelated_time - state->last_hit > enemy_hit_cooldown)
        {
            state->last_hit = state->accumelated_time;
            state->health -= 35.0f;
            hit = true;
        }

        /* NOTE: collision with map */
        for (u32 j = 0; j < state->test_map.wall_count; j++)
        {
            aabb_collision_result collision = aabb_collision(rect(enemy->position, enemy->size), state->test_map.walls[j].bounding_box);
            if (collision.is_colliding)
            {
                enemy->position = vector2_add(enemy->position, collision.penetration_depth);
            }
        }
    }

    state->accumelated_time += delta_time;

    /* NOTE: draw map */
    for (u32 i = 0; i < state->test_map.wall_count; i++)
    {
        map_wall *wall = &state->test_map.walls[i];
        render_draw_rectangle(render_buffer, wall->bounding_box, wall->color, UNIT, UNTEXTURED);
    }

    for (u32 i = 0; i < state->enemy_count; i++)
    {
        enemy *enemy = &state->enemies[i];
        rectangle enemy_bounds = rect(enemy->position, enemy->size);
        render_draw_rectangle(render_buffer, enemy_bounds, WHITE, UNIT, 2);
    }

    /* NOTE: player */
    render_draw_rectangle(render_buffer, player, GREEN, UNIT, UNTEXTURED);
    render_draw_rectangle(render_buffer, player, WHITE, UNIT, 1);

    if (state->health < 0.0f || hit)
    {
        render_draw_rectangle(render_buffer, rect(v2(0, 0), v2(10000, 10000)), RED, UNIT, UNTEXTURED);
    }
    if (ray_hit)
    {
        render_draw_line(render_buffer, ray_start, ray_end, GREEN);
    }
    else
    {
        render_draw_line(render_buffer, ray_start, vector2_add(ray_start, vector2_scale(vector2_norm(ray_direction), 3000.0f)), RED);
    }
}

GET_SOUND_SAMPLES(get_sound_samples)
{
    UNUSED(sound_buffer);
}
