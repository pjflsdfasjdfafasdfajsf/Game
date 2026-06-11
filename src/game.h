#pragma once

#include "game_map.h"
#include "game_platform.h"
#include "game_types.h"

typedef struct
{
    vector2 position;
    vector2 size;
} enemy;

typedef struct
{
    vector2 position;
    f32 health;
    f32 last_hit;

    enemy enemies[4096];
    u32 enemy_count;

    map test_map;

    // NOTE: is never resetted
    f64 accumelated_time;
} game_state;

/** NOTE: function definitions below are for release builds where the game is not
 * built as a separate dynamic library but rather just as one executable with
 * the platform
 */

GET_SOUND_SAMPLES(get_sound_samples);
UPDATE_AND_RENDER(update_and_render);
