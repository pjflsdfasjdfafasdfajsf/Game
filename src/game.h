#pragma once

#include "game_platform.h"

typedef struct
{
    vector2 position;
    vector2 velocity;
    vector2 accelaration;
    vector2 player_size;
    bool look_right;
    bool on_ground;
    u32 element;
    f32 accumulated_time;
} game_state;

/** NOTE: function definitions below are for release builds where the game is not
 * built as a separate dynamic library but rather just as one executable with
 * the platform
 */

GET_SOUND_SAMPLES(get_sound_samples);
UPDATE_AND_RENDER(update_and_render);
