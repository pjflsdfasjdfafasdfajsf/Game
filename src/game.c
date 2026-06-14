#include "game.h"
#include "game_map.h"
#include "game_math.h"
#include "game_platform.h"
#include "game_png.h"
#include "game_types.h"
#include <SDL3/SDL_pixels.h>

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

UPDATE_AND_RENDER(game_mode_play_update_and_render)
{
    UNUSED(platform); // for now
    game_state *state = (game_state *)memory->permanent_arena.base_pointer;
    if (!memory->is_initialized)
    {
        return;
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
        enemy_add(state, input->mouse_position, v2(state->health * 0.2f, state->health * 0.2f));
    }

    if (button_pressed(input->keys[key_code_m]))
    {
        state->game_mode = GAME_MODE_EDITOR;
        state->test_map = map_create("test.map");
    }

    rectangle player = rect(state->position, v2(20.0f, 20.0f));

    /* NOTE: remove later only here for the ray visualisation on the map */
    bool ray_hit = false;
    vector2 ray_start;
    vector2 ray_end;
    vector2 ray_direction;
    f32 ray_distance = FLOAT_MAX;

    /* NOTE: player collisions with the map */
    for (u32 i = 0; i < state->test_map.wall_count; i++)
    {
        map_wall *wall = &state->test_map.walls[i];

        aabb_collision_result collision = aabb_collision(player, wall->bounding_box);
        if (collision.is_colliding)
        {
            state->position = vector2_add(state->position, collision.penetration_depth);
            player = rect(state->position, v2(20.0f, 20.0f));
        }

        vector2 start = v2(state->position.x + 10.0f, state->position.y + 10.0f);
        vector2 direction = vector2_sub(input->mouse_position, start);
        raycast_result ray = ray_intersect_rectangle_infinite(start, direction, wall->bounding_box);

        /* NOTE: doesnt check the nearest collision at the moment so the ray might hit through another object */
        if (ray.is_hitting)
        {
            ray_hit = true;

            /* NOTE: render the ray on the closest object */
            f32 distance = vector2_length(vector2_sub(ray.hit_position, ray_start));
            if (distance < ray_distance)
            {
                ray_end = ray.hit_position;
                ray_distance = distance;
            }
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
        render_draw_rectangle(render_buffer, wall->bounding_box, wall->color, TEXCOORDS_UNIT, UNTEXTURED);
    }

    for (u32 i = 0; i < state->enemy_count; i++)
    {
        enemy *enemy = &state->enemies[i];
        rectangle enemy_bounds = rect(enemy->position, enemy->size);
        render_draw_rectangle(render_buffer, enemy_bounds, WHITE, TEXCOORDS_UNIT, 2);
    }

    /* NOTE: player */
    render_draw_rectangle(render_buffer, player, GREEN, TEXCOORDS_UNIT, UNTEXTURED);
    render_draw_rectangle(render_buffer, player, WHITE, TEXCOORDS_UNIT, 1);

    if (state->health < 0.0f || hit)
    {
        render_draw_rectangle(render_buffer, rect(v2(0, 0), v2(10000, 10000)), RED, TEXCOORDS_UNIT, UNTEXTURED);
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

static void get_gizmo_rectangles(map_wall *wall, rectangle *arrow_up, rectangle *arrow_right, rectangle *arrow_down, rectangle *arrow_left)
{    
    const f32 spacing = 20.0f;
    f32 max_size = MIN(wall->bounding_box.width, wall->bounding_box.height) * 0.25f;
    
    /* NOTE: achive correct aspect ration for the gizmo image 20x100 */
    const f32 aspect_ratio = 20.0f / 100.0f; 
    f32 other_size = max_size * aspect_ratio;

    if (arrow_up)
    {
        arrow_up->dimensions = v2(other_size, max_size);
        arrow_up->position = v2(wall->bounding_box.x + wall->bounding_box.width * 0.5f - arrow_up->width * 0.5f, wall->bounding_box.y + wall->bounding_box.height * 0.5f - arrow_up->height - spacing);
    }
    
    if (arrow_right)
    {
        arrow_right->dimensions = v2(max_size, other_size); 
        arrow_right->position = v2(wall->bounding_box.x + wall->bounding_box.width * 0.5f + spacing, wall->bounding_box.y + wall->bounding_box.height * 0.5f - arrow_right->height * 0.5f);
    }

    if (arrow_down)
    {
        arrow_down->dimensions = v2(other_size, max_size);              
        arrow_down->position = v2(wall->bounding_box.x + wall->bounding_box.width * 0.5f - arrow_down->width * 0.5f, wall->bounding_box.y + wall->bounding_box.height * 0.5f + spacing);
    }

    if (arrow_left)
    {
        arrow_left->dimensions = v2(max_size, other_size);
        arrow_left->position = v2(wall->bounding_box.x + wall->bounding_box.width * 0.5f - arrow_left->width - spacing, wall->bounding_box.y + wall->bounding_box.height * 0.5f - arrow_left->height * 0.5f);
    }   
}

UPDATE_AND_RENDER(game_mode_editor_update_and_render)
{
    UNUSED(delta_time); UNUSED(platform);

    game_state *state = (game_state *)memory->permanent_arena.base_pointer;
    if (!memory->is_initialized)
    {
        return;
    }

    /* NOTE: switch back to GAME_MODE_PLAY */
    if (button_pressed(input->keys[key_code_m]))
    {
        state->game_mode = GAME_MODE_PLAY;
        map_write(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, platform, &state->test_map);
    }

    /* NOTE: switch to move tool */
    if (button_pressed(input->keys[key_code_1]))
    {
        state->selection_mode = SELECTION_MODE_MOVE;
    }

    /* NOTE: switch to scale tool */
    if (button_pressed(input->keys[key_code_2]))
    {
        state->selection_mode = SELECTION_MODE_SCALE;
    }

    /* NOTE: select a wall */
    if (button_pressed(input->mouse_buttons[mouse_button_left]))
    {
        vector2 selection_dimension = v2(2, 2);
        vector2 pos = vector2_sub(input->mouse_position, vector2_scale(selection_dimension, 0.5f));

        bool on_wall = false;
        for (u32 i = 0; i < state->test_map.wall_count; i++)
        {
            aabb_collision_result collision = aabb_collision(rect(pos, selection_dimension), state->test_map.walls[i].bounding_box);
            if (collision.is_colliding)
            {
                state->selected_wall = i;
                on_wall = true;
                break; 
            }   
        }

        if (!on_wall)
        {
            state->selected_wall = NO_WALL;
        }
    }

    /* NOTE: creating new wall */
    if (button_pressed(input->mouse_buttons[mouse_button_right]))
    {
        state->start_press = input->mouse_position;
        state->rectangle_press = true;
    }
    if (button_released(input->mouse_buttons[mouse_button_right]))
    {
        state->rectangle_press = false;

        vector2 top_left = v2(MIN(state->start_press.x, input->mouse_position.x), MIN(state->start_press.y, input->mouse_position.y));
        
        vector2 dimensions = vector2_sub(input->mouse_position, state->start_press);
        dimensions = vector2_abs(dimensions);

        map_add(&state->test_map, rect(top_left, dimensions), GREEN);
    }

    if (button_held(input->keys[key_code_shift]) && state->selected_wall != NO_WALL)
    {
        vector2 mouse_delta = vector2_sub(input->mouse_position, input->last_mouse_position);

        ASSERT(state->selected_wall < state->test_map.wall_count);

        map_wall *wall = &state->test_map.walls[state->selected_wall];
        wall->bounding_box.x += mouse_delta.x;
        wall->bounding_box.y += mouse_delta.y;
    }

    if (state->rectangle_press)
    {
        vector2 dimensions = vector2_sub(input->mouse_position, state->start_press);
        render_draw_rectangle(render_buffer, rect(state->start_press, dimensions), WHITE, TEXCOORDS_UNIT, UNTEXTURED);
    }

    /* NOTE: draw map */
    for (u32 i = 0; i < state->test_map.wall_count; i++)
    {
        map_wall *wall = &state->test_map.walls[i];

        vector4 color = state->selected_wall == i ? YELLOW : wall->color;
        
        render_draw_rectangle(render_buffer, wall->bounding_box, color, TEXCOORDS_UNIT, UNTEXTURED);
    }

    /* NOTE: draw gizmo */
    if (state->selected_wall != NO_WALL)
    {
        u32 texture = state->selection_mode == SELECTION_MODE_MOVE ? 3 : 4;
        
        ASSERT(state->selected_wall < state->test_map.wall_count);
        map_wall *wall = &state->test_map.walls[state->selected_wall];
        
        rectangle arrow_up, arrow_right;
        get_gizmo_rectangles(wall, &arrow_up, &arrow_right, 0, 0);

        render_draw_rectangle(render_buffer, arrow_up, WHITE, TEXCOORDS_UNIT, texture);
        render_draw_rectangle(render_buffer, arrow_right, WHITE, texcoords_rotate_90_clockwise(TEXCOORDS_UNIT), texture);

        if (state->selection_mode == SELECTION_MODE_SCALE)
        {
            rectangle arrow_down, arrow_left;
            get_gizmo_rectangles(wall, 0, 0, &arrow_down, &arrow_left);

            render_draw_rectangle(render_buffer, arrow_down, WHITE, texcoords_rotate_180(TEXCOORDS_UNIT), texture);
            render_draw_rectangle(render_buffer, arrow_left, WHITE, texcoords_rotate_90_counter_clockwise(TEXCOORDS_UNIT), texture);
        }
    }
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

   static const char gizmo_move[] = {
#embed "../assets/images/gizmo_move.png"
        };

        image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, gizmo_move, sizeof(gizmo_move));
        render_allocate_texture(render_buffer, 3, image.size, image.pixels);

   static const char gizmo_scale[] = {
#embed "../assets/images/gizmo_scale.png"
        };

        image = image_load_from_png(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, gizmo_scale, sizeof(gizmo_scale));
        render_allocate_texture(render_buffer, 4, image.size, image.pixels);

        /* NOTE: change path of the map to be in assets?
         * im not sure because its right now just for testing
         * so i will keep it here for now */
        static const char map_data[] = {
#embed "../test.map"
        };

        map_load(&memory->permanent_arena, &memory->temporary_arena, memory->standard_error_stream, map_data, sizeof(map_data), &state->test_map);

        state->game_mode = GAME_MODE_PLAY;

        /* NOTE: GAME_MODE_PLAY */
        state->position = v2(10, 10);
        state->health = 100.0f;
        state->last_hit = 0.0f;
        state->enemy_count = 0;

        /* NOTE: GAME_MODE_EDITOR */
        state->selected_wall = NO_WALL;
        state->selected_direction = SELECTED_DIRECTION_NONE;
        
        state->accumelated_time = 0.0;
        memory->is_initialized = true;
    }

    switch (state->game_mode)
    {
    case GAME_MODE_PLAY:
    {
        game_mode_play_update_and_render(memory, input, platform, render_buffer, delta_time);
    } break;
    case GAME_MODE_EDITOR:
    {
        game_mode_editor_update_and_render(memory, input, platform, render_buffer, delta_time);
    } break;
    }
}

GET_SOUND_SAMPLES(get_sound_samples)
{
    UNUSED(sound_buffer);
}
