#pragma once

#include "game_map.h"
#include "game_platform.h"
#include "game_types.h"

#define NO_WALL UINT32_MAX

typedef struct
{
    vector2 position;
    vector2 size;
} enemy;

typedef enum
{
    GAME_MODE_PLAY,
    GAME_MODE_EDITOR,
} game_mode;

typedef enum
{
    SELECTION_MODE_MOVE,
    SELECTION_MODE_SCALE,
} selection_mode;

typedef enum
{
    SELECTED_DIRECTION_NONE, /* NOTE: gets used as the state when no gizmo arrow is selected */
    SELECTED_DIRECTION_UP,
    SELECTED_DIRECTION_RIGHT,
    SELECTED_DIRECTION_CENTER,
} selected_direction;

typedef struct
{
    game_mode game_mode;

    /* NOTE: game */
    vector2 position;
    f32 health;
    f32 last_hit;

    enemy enemies[4096];
    u32 enemy_count;

    /* NOTE: editor */
    vector2 start_press;
    f32 scale_center_collider_radius;
    u32 selected_wall;
    selection_mode selection_mode;
    selected_direction selected_direction;

    /* NOTE: draws the rectangle when dragging to create new map walls */
    bool rectangle_press;

    /* NOTE: first time the direction got changed to SELECTED_DIRECTION_NONE before it repeats before it repeats */
    bool first_no_selected_direction;

    /* NOTE: shared */
    map test_map;

    /* NOTE: is NEVER resetted */
    f64 accumelated_time;
} game_state;

/** NOTE: function definitions below are for release builds where the game is not
 * built as a separate dynamic library but rather just as one executable with
 * the platform
 */

GET_SOUND_SAMPLES(get_sound_samples);
UPDATE_AND_RENDER(update_and_render);
