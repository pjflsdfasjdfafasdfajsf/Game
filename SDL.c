#include <stdlib.h>

#include "game/game_platform.h"
#include "SDL.h"

/** TODO:
 * 1. finish this base platform layer
 * 2. create SDL_gpu.c/SDL_gpu.h and implement them
 *  FOR SHADERS: !!!
 *      Shadercross is very bloated but it has a binary that allows you to compile shader source file to any bytecode we need, such
 *      as MSL, SPIRV, et cetera. SDL_ttf does so: https://github.com/libsdl-org/SDL_ttf/tree/main/examples 
 *      So for sake of shader hot-reloadability in Debug we will just link Shadercross regularly, and in Release later we can just do whatever I 
 *      mentioned above.
 * 3. remove linux_* files
 * 4. update build script
 */

bool SDL_initialize() {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        SDL_Log("error: failed to initialize video subsystem: %s\n", SDL_GetError());

        return false;
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        SDL_Log("error: failed to initialize audio subsystem: %s\n", SDL_GetError());

        return false;
    }

    return true;
}

/****************************************************
 * NOTE: game loading
 ****************************************************/

/** TODO: in release mode the game should be compiled into a single executable */

void code_deinitialize(code *code) {
    if (code->handle) {
        SDL_UnloadObject(code->handle);
    }
}

bool code_initialize(code *code) {
    code_deinitialize(code);

    SDL_PathInfo info;
    /** TODO: make it so user cwd does not matter when opening these */
    if (SDL_GetPathInfo("./game.so", &info)) {
        code->last_modification_time = info.modify_time;
        /** NOTE: copy it into a temporary file just in case so compiler does not crash us */
        SDL_CopyFile("./game.so", "./game.tmp.so");
    } else {
        /** NOTE: file does not exist? */
        SDL_Log("error: game.so: %s\n", SDL_GetError());

        return false;
    }

    code->handle = SDL_LoadObject("./game.tmp.so");
    if (code->handle) {
        code->update_and_render = (update_and_render_function *)SDL_LoadFunction(code->handle, "update_and_render");
        if (!code->update_and_render) {
            SDL_Log("error: game binary is missing the required update_and_render procedure\n");

            return false;
        }
    } else {
        SDL_Log("error: failed to load game binary: %s\n", SDL_GetError());

        return false;
    }

    return true;
}

void code_update(code *code) {
    SDL_PathInfo info;
    if (SDL_GetPathInfo("game.so", &info)) {
        if (info.modify_time > code->last_modification_time) {
            code_initialize(code);
        }
    }
}

/****************************************************
 * NOTE: window
 ****************************************************/

bool window_initialize() {
    SDL_Window *window = SDL_CreateWindow("Game", 1280, 720, SDL_WINDOW_VULKAN);
    if (!window) {
        SDL_Log("error: failed to create window: %s\n", SDL_GetError());

        return false;
    }

    return true;
}

void window_update() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {

        case SDL_EVENT_QUIT: {
            exit(EXIT_SUCCESS);
        } break;
        }
    }
}

/****************************************************
 * NOTE: sound
 ****************************************************/

bool sound_initialize(state *state) {
    SDL_AudioSpec spec = {0};
    spec.channels = 1;
    spec.format = SDL_AUDIO_F32;
    spec.freq = 48000;

    state->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, 0, 0);
    if (!state->stream) {
        SDL_Log("error: failed to open audio stream: %s\n", SDL_GetError());

        return false;
    }

    /** NOTE: we need to do this since audio streams are paused by default for some reason */
    SDL_ResumeAudioStreamDevice(state->stream);

    return true;
}

void sound_update(state *state) {
    // const int bytes = (48000 * sizeof(float)) / 4;

    // while (SDL_GetAudioStreamQueued(state->stream) < bytes) {
    //     float samples[512] = {0};

    //     for (int i = 0; i < 512; i++) {
    //         float phase = (float)state->index * 440.0f / 48000.0f;
    //         samples[i] = SDL_sinf(phase * 2.0f * SDL_PI_F);
    //         state->index++;
    //     }

    //     state->index %= 48000;

    //     SDL_PutAudioStreamData(state->stream, samples, sizeof(samples));
    // }
}

/****************************************************/

void update(state *state, game_memory *memory, render_command_buffer *command_buffer) {
    while (true) {
        window_update();
        code_update(&state->code);
        if (state->code.update_and_render) {
            state->code.update_and_render(memory, command_buffer);
        }
        sound_update(state);
    }
}

int main() {
    state state = {0};
    SDL_initialize();

    /** NOTE: main initialization */

    if (!window_initialize()) {
        return EXIT_FAILURE;
    }
    if (!sound_initialize(&state)) {
        return EXIT_FAILURE;
    }
    if (!code_initialize(&state.code)) {
        return EXIT_FAILURE;
    }

    /** NOTE: memory initialization */

    usize permanent_arena_size = MEGABYTES(64);
    usize temporary_arena_size = MEGABYTES(256);

    void *permanent_memory_block = SDL_malloc(permanent_arena_size);
    void *temporary_memory_block = SDL_malloc(temporary_arena_size);

    game_memory memory = {0};
    game_memory_initialize(&memory, permanent_memory_block, permanent_arena_size, temporary_memory_block, temporary_arena_size);

    /***/

    render_command_buffer command_buffer;
    usize command_buffer_capacity = MEGABYTES(4);

    render_command_buffer_initialize(&command_buffer, MEMORY_ARENA_PUSH_BYTES(&memory.temporary_arena, command_buffer_capacity), command_buffer_capacity);

    update(&state, &memory, &command_buffer);
}