#pragma once

#include <SDL3/SDL.h>

#include "game/game_platform.h"

typedef struct {
    SDL_SharedObject *handle;
    SDL_Time last_modification_time;

    update_and_render_function *update_and_render;
} code;

typedef struct {
    SDL_AudioStream *stream;
    code code;
} state;
