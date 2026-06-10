#include <stdlib.h>

#include "SDL.h"
#include "SDL_gpu.h"
#include "game_platform.h"

/** TODO:
 *  FOR SHADERS: !!!
 *      Shadercross is very bloated but it has a binary that allows you to
 * compile shader source file to any bytecode we need, such as MSL, SPIRV, et
 * cetera. SDL_ttf does so:
 * https://github.com/libsdl-org/SDL_ttf/tree/main/examples So for sake of
 * shader hot-reloadability in Debug we will just link Shadercross regularly,
 * and in Release later we can just do whatever I mentioned above.
 */

bool SDL_initialize(void)
{
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
    {
        SDL_Log("error: failed to initialize video subsystem: %s\n", SDL_GetError());

        return false;
    }

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        SDL_Log("error: failed to initialize audio subsystem: %s\n", SDL_GetError());

        return false;
    }

    return true;
}

/****************************************************
 * NOTE: input
 ****************************************************/

static key_code input_map(SDL_Scancode scancode)
{
    switch (scancode)
    {
    case SDL_SCANCODE_W:
        return key_code_w;
    case SDL_SCANCODE_A:
        return key_code_a;
    case SDL_SCANCODE_S:
        return key_code_s;
    case SDL_SCANCODE_D:
        return key_code_d;
    default:
        return key_code_none;
    }
}

void input_update_pre_event(input *input)
{
    for (u32 key_index = 0; key_index < key_code_count; key_index++)
    {
        input->keys[key_index].was_down = input->keys[key_index].is_down;
    }
    for (u32 button_index = 0; button_index < mouse_button_count; button_index++)
    {
        input->mouse_buttons[button_index].was_down = input->mouse_buttons[button_index].is_down;
    }
    input->mouse_scroll = 0.0f;
}

void input_handle_event(input *input, SDL_Event *event)
{
    switch (event->type)
    {
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP:
    {
        key_code code = input_map(event->key.scancode);
        if (code != key_code_none)
        {
            input->keys[code].is_down = (event->type == SDL_EVENT_KEY_DOWN);
        }
    }
    break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP:
    {
        mouse_button button = mouse_button_count;
        if (event->button.button == SDL_BUTTON_LEFT)
        {
            button = mouse_button_left;
        }
        else if (event->button.button == SDL_BUTTON_RIGHT)
        {
            button = mouse_button_right;
        }
        else if (event->button.button == SDL_BUTTON_MIDDLE)
        {
            button = mouse_button_middle;
        }

        if (button != mouse_button_count)
        {
            input->mouse_buttons[button].is_down = (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN);
        }
    }
    break;

    case SDL_EVENT_MOUSE_MOTION:
    {
        input->mouse_position.x = event->motion.x;
        input->mouse_position.y = event->motion.y;
    }
    break;

    case SDL_EVENT_MOUSE_WHEEL:
    {
        input->mouse_scroll += event->wheel.y;
    }
    break;
    }
}

/****************************************************
 * NOTE: game loading
 ****************************************************/

/** TODO: in release mode the game should be compiled into a single
executable */

void code_deinitialize(code *code)
{
    if (code->handle)
    {
        SDL_UnloadObject(code->handle);
    }
}

bool code_initialize(code *code)
{
    code_deinitialize(code);

    SDL_PathInfo info;
    /** TODO: make it so user cwd does not matter when opening these */
    if (SDL_GetPathInfo("./Game.so", &info))
    {
        code->last_modify_time = info.modify_time;
        /** NOTE: copy it into a temporary file just in case so compiler does
        not crash us */
        SDL_CopyFile("./Game.so", "./Game.tmp.so");
    }
    else
    {
        /** NOTE: file does not exist? */
        SDL_Log("error: Game.so: %s\n", SDL_GetError());

        return false;
    }

    code->handle = SDL_LoadObject("./Game.tmp.so");
    if (code->handle)
    {
        code->update_and_render = (update_and_render_function *)SDL_LoadFunction(code->handle, "update_and_render");
        if (!code->update_and_render)
        {
            SDL_Log("error: game binary is missing the required procedure (update_and_render)\n");
        }

        code->get_sound_samples = (get_sound_samples_function *)SDL_LoadFunction(code->handle, "get_sound_samples");
        if (!code->get_sound_samples)
        {
            SDL_Log("error: game binary is missing the required procedure (get_sound_samples)\n");
        }
    }
    else
    {
        SDL_Log("error: failed to load game binary: %s\n", SDL_GetError());

        return false;
    }

    return true;
}

void code_update(code *code)
{
    SDL_PathInfo info;
    if (SDL_GetPathInfo("Game.so", &info))
    {
        if (info.modify_time > code->last_modify_time)
        {
            /** NOTE: this is needed so it does not short read and segfault */
            SDL_Delay(50);
            code_initialize(code);
        }
    }
}

/****************************************************
 * NOTE: window
 ****************************************************/

bool window_initialize(state *state)
{
    state->window = SDL_CreateWindow("Game", 1280, 720, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
    if (!state->window)
    {
        SDL_Log("error: failed to create window: %s\n", SDL_GetError());

        return false;
    }

    return true;
}

void window_update(input *input)
{
    input_update_pre_event(input);

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
        {
            exit(EXIT_SUCCESS);
        }
        break;

        default:
        {
            input_handle_event(input, &event);
        }
        break;
        }
    }
}

/****************************************************
 * NOTE: sound
 ****************************************************/

bool sound_initialize(state *state)
{
    SDL_AudioSpec spec = {0};
    spec.channels = 1;
    spec.format = SDL_AUDIO_F32;
    spec.freq = 48000;

    state->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, 0, 0);
    if (!state->stream)
    {
        SDL_Log("error: failed to open audio stream: %s\n", SDL_GetError());

        return false;
    }

    /** NOTE: we need to do this since audio streams are paused by default for
     * some reason */
    SDL_ResumeAudioStreamDevice(state->stream);

    return true;
}

void sound_update(state *state, sound_buffer *sound_buffer)
{
    const u32 bytes_per_frame = sound_buffer->channel_count * sizeof(f32);
    const u32 target_queued_bytes = sound_buffer->samples_per_second * bytes_per_frame / 4;

    u32 queued_bytes = SDL_GetAudioStreamQueued(state->stream);
    if (queued_bytes >= target_queued_bytes)
    {
        return;
    }

    u32 frames_to_write = (target_queued_bytes - queued_bytes) / bytes_per_frame;
    sound_buffer->frame_count = frames_to_write;

    if (state->code.get_sound_samples)
    {
        state->code.get_sound_samples(sound_buffer);
    }

    SDL_PutAudioStreamData(state->stream, sound_buffer->samples, frames_to_write * bytes_per_frame);
}

/****************************************************/

void update(state *state, gpu *gpu, memory *memory, input *input, platform *platform, render_buffer *render_buffer, sound_buffer *sound_buffer)
{
    u64 last_counter = SDL_GetPerformanceCounter();
    u64 counter_frequency = SDL_GetPerformanceFrequency();

    while (true)
    {
        u64 current_counter = SDL_GetPerformanceCounter();
        f32 delta_time = (f32)(current_counter - last_counter) / (f32)counter_frequency;
        last_counter = current_counter;

        window_update(input);

        render_buffer_reset(render_buffer);

        if (memory->standard_info_stream)
        {
            memory->standard_info_stream->offset = 0;
        }
        if (memory->standard_error_stream)
        {
            memory->standard_error_stream->offset = 0;
        }

        code_update(&state->code);
        if (state->code.update_and_render)
        {
            state->code.update_and_render(memory, input, platform, render_buffer, delta_time);
        }

        if (memory->standard_info_stream && memory->standard_info_stream->offset > 0)
            SDL_Log("%.*s", (int)memory->standard_info_stream->offset, memory->standard_info_stream->memory);
        if (memory->standard_error_stream && memory->standard_error_stream->offset > 0)
            SDL_Log("%.*s", (int)memory->standard_error_stream->offset, memory->standard_error_stream->memory);

        sound_update(state, sound_buffer);
        gpu_update(gpu, render_buffer);
    }
}

int main(void)
{
    state state = {0};
    SDL_initialize();

    if (!window_initialize(&state))
    {
        return EXIT_FAILURE;
    }
    if (!sound_initialize(&state))
    {
        return EXIT_FAILURE;
    }

    gpu gpu = {0};
    if (!gpu_initialize(&gpu, state.window))
    {
        return EXIT_FAILURE;
    }

    if (!code_initialize(&state.code))
    {
        return EXIT_FAILURE;
    }

    usize permanent_memory_size = MEGABYTES(64);
    usize temporary_memory_size = MEGABYTES(256);
    usize render_buffer_size = MEGABYTES(4);
    usize standard_stream_size = KILOBYTES(4);

    void *permanent_arena_memory = SDL_calloc(1, permanent_memory_size);
    void *temporary_arena_memory = SDL_calloc(1, temporary_memory_size);
    void *render_buffer_memory = SDL_calloc(1, render_buffer_size);
    void *info_stream_memory = SDL_calloc(1, standard_stream_size);
    void *error_stream_memory = SDL_calloc(1, standard_stream_size);

    if (!permanent_arena_memory || !temporary_arena_memory || !render_buffer_memory || !info_stream_memory || !error_stream_memory)
    {
        SDL_Log("error: failed to allocate game memory\n");
        return EXIT_FAILURE;
    }

    memory memory = {0};
    memory_arena_initialize(&memory.permanent_arena, permanent_arena_memory, permanent_memory_size);
    memory_arena_initialize(&memory.temporary_arena, temporary_arena_memory, temporary_memory_size);

    memory_stream info_stream = {0};
    memory_stream error_stream = {0};
    memory_stream_initialize_writable(&info_stream, info_stream_memory, standard_stream_size);
    memory_stream_initialize_writable(&error_stream, error_stream_memory, standard_stream_size);

    memory.standard_info_stream = &info_stream;
    memory.standard_error_stream = &error_stream;
    memory.is_initialized = false;

    render_buffer render_buffer = {0};
    render_buffer_initialize(&render_buffer, render_buffer_memory, render_buffer_size);

    const u32 sound_samples_per_second = 48000;
    const u32 sound_channel_count = 1;
    const u32 sound_max_frames = sound_samples_per_second / 4;

    void *sound_samples_memory = SDL_calloc(sound_max_frames * sound_channel_count, sizeof(f32));
    if (!sound_samples_memory)
    {
        SDL_Log("error: failed to allocate sound buffer\n");
        return EXIT_FAILURE;
    }

    sound_buffer sound_buffer = {0};
    sound_buffer.samples_per_second = sound_samples_per_second;
    sound_buffer.channel_count = sound_channel_count;
    sound_buffer.samples = (f32 *)sound_samples_memory;

    input input = {0};

    platform platform = {
        /* NOTE: no point in in making own functions that wrap SDL */ 
        .file_save = SDL_SaveFile,
    };

    update(&state, &gpu, &memory, &input, &platform, &render_buffer, &sound_buffer);

    return EXIT_SUCCESS;
}
