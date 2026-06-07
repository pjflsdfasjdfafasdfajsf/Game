#pragma once

#include "game_platform.h"

#include <SDL3/SDL.h>

typedef struct
{
    SDL_SharedObject *handle;
    SDL_Time last_modify_time;

    update_and_render_function *update_and_render;
    get_sound_samples_function *get_sound_samples;
} code;

typedef struct
{
    SDL_Window *window;
    SDL_AudioStream *stream;

    code code;
} state;
