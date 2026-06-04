#include <alsa/asoundlib.h>
#include <pthread.h>
#include <limits.h>
#include <dlfcn.h>

#include "linux.h"
#include "linux_vulkan.h"
#include "game_platform.h"
#include "game_types.h"

#if defined(DEBUG)
static void *game_so = 0;
static update_and_render_function *GAME_UPDATE_AND_RENDER = 0;
static get_sound_samples_function *GAME_GET_SOUND_SAMPLES = 0;

static void absolute_libary_path(const char *library_file_name, char *destination_buffer, usize destination_capacity) {
    memory_zero(destination_buffer, destination_capacity);

    char executable_path[PATH_MAX];
    isize executable_path_length = readlink("/proc/self/exe", executable_path, sizeof(executable_path) - 1);

    if (executable_path_length == -1) {
        printf("ERROR: readlink failed.\n");

        return;
    }

    executable_path[executable_path_length] = '\0';

    isize final_slash_index = string_find_last_occurrence_of_character(executable_path, '/');

    if (final_slash_index != -1) {
        executable_path[final_slash_index + 1] = '\0';
    }

    string_copy(destination_buffer, destination_capacity, executable_path);
    string_append(destination_buffer, destination_capacity, library_file_name);
}

static bool copy_file(const char *source_path, const char *destination_path) {
    int source_file = open(source_path, O_RDONLY);
    if (source_file < 0) {
        return false;
    }

    int destination_file = open(destination_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (destination_file < 0) {
        close(source_file);
        return false;
    }

    char buffer[4096];

    while (true) {
        isize bytes_read = read(source_file, buffer, sizeof(buffer));

        if (bytes_read == 0) {
            break;
        }

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        char *write_pointer = buffer;
        isize bytes_remaining = bytes_read;

        while (bytes_remaining > 0) {
            isize bytes_written = write(destination_file, write_pointer, bytes_remaining);

            if (bytes_written >= 0) {
                bytes_remaining -= bytes_written;
                write_pointer += bytes_written;
            } else if (errno != EINTR) {
                return false;
            }
        }
    }

    close(source_file);

    if (close(destination_file) < 0) {
        return false;
    }

    return true;
}

static void game_code_load(void) {
    if (game_so) {
        dlclose(game_so);

        game_so = 0;
        GAME_UPDATE_AND_RENDER = 0;
        GAME_GET_SOUND_SAMPLES = 0;
    }

    char source_library_path[PATH_MAX];
    char temporary_library_path[PATH_MAX];

    absolute_libary_path("../lib/Game.so", source_library_path, sizeof(source_library_path));
    absolute_libary_path("libgame.temp.so", temporary_library_path, sizeof(temporary_library_path));

    if (!copy_file(source_library_path, temporary_library_path)) {
        fprintf(stderr, "could not copy Game.so.\n");
    }

    game_so = dlopen(temporary_library_path, RTLD_NOW);
    if (!game_so) {
        fprintf(stderr, "%s\n", dlerror());
    }

    GAME_UPDATE_AND_RENDER = (update_and_render_function *)dlsym(game_so, "update_and_render");
    GAME_GET_SOUND_SAMPLES = (get_sound_samples_function *)dlsym(game_so, "get_sound_samples");
}

#else
#define GAME_UPDATE_AND_RENDER update_and_render
#define GAME_GET_SOUND_SAMPLES get_sound_samples
#endif

static void xdg_toplevel_configure_handler(void *user_data, struct xdg_toplevel *xdg_toplevel, int32_t width, int32_t height, struct wl_array *states) {
    UNUSED(xdg_toplevel), UNUSED(width), UNUSED(height);

    linux_wayland *wayland = (linux_wayland *)user_data;

    if (!wayland) {
        return;
    }

    bool is_activated = false;
    uint32_t *state;
    wl_array_for_each(state, states) {
        if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) {
            is_activated = true;
        }
    }

    if (width > 0) {
        wayland->width = width;
    }
    if (height > 0) {
        wayland->height = height;
    }

    wayland->is_focused = is_activated;
}

static void xdg_toplevel_close_handler(void *user_data, struct xdg_toplevel *xdg_toplevel) {
    UNUSED(xdg_toplevel);

    linux_wayland *wayland = (linux_wayland *)user_data;

    if (wayland) {
        wayland->is_running = false;
    }
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure_handler,
    .close = xdg_toplevel_close_handler,
};

static void xdg_surface_configure_handler(void *user_data, struct xdg_surface *xdg_surface, uint32_t serial) {
    linux_wayland *wayland = (linux_wayland *)user_data;

    xdg_surface_ack_configure(xdg_surface, serial);

    if (wayland) {
        wayland->is_configured = true;
    }
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure_handler,
};

static void xdg_window_manager_base_ping_handler(void *user_data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    UNUSED(user_data);

    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_window_manager_base_ping_handler,
};

static void registry_global_handler(void *user_data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    UNUSED(version);

    linux_wayland *wayland = (linux_wayland *)user_data;

    usize compositor_name_length = string_get_length(wl_compositor_interface.name);
    usize xdg_wm_base_name_length = string_get_length(xdg_wm_base_interface.name);

    bool is_compositor = memory_equals(interface, wl_compositor_interface.name, compositor_name_length + 1);
    bool is_xdg_wm_base = memory_equals(interface, xdg_wm_base_interface.name, xdg_wm_base_name_length + 1);

    if (is_compositor) {
        wayland->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (is_xdg_wm_base) {
        wayland->xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(wayland->xdg_wm_base, &xdg_wm_base_listener, wayland);
    }
}

static void registry_global_remove_handler(void *user_data, struct wl_registry *registry, uint32_t name) {
    UNUSED(user_data), UNUSED(registry), UNUSED(name);
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global_handler,
    .global_remove = registry_global_remove_handler,
};

linux_wayland window_create(const char *title) {
    linux_wayland wayland = {0};

    wayland.width = DEFAULT_WINDOW_WIDTH;
    wayland.height = DEFAULT_WINDOW_HEIGHT;

    wayland.display = wl_display_connect(0);
    if (!wayland.display) {
        fprintf(stderr, "could not connect to wayland display.\n");
        return wayland;
    }

    wayland.registry = wl_display_get_registry(wayland.display);
    wl_registry_add_listener(wayland.registry, &registry_listener, &wayland);

    wl_display_roundtrip(wayland.display);

    if (!wayland.compositor || !wayland.xdg_wm_base) {
        fprintf(stderr, "failed to bind wayland compositor or xdg_wm_base.\n");
        return wayland;
    }

    wayland.surface = wl_compositor_create_surface(wayland.compositor);
    wayland.xdg_surface = xdg_wm_base_get_xdg_surface(wayland.xdg_wm_base, wayland.surface);
    xdg_surface_add_listener(wayland.xdg_surface, &xdg_surface_listener, &wayland);

    wayland.xdg_toplevel = xdg_surface_get_toplevel(wayland.xdg_surface);
    xdg_toplevel_add_listener(wayland.xdg_toplevel, &xdg_toplevel_listener, &wayland);

    xdg_toplevel_set_title(wayland.xdg_toplevel, title);
    xdg_toplevel_set_app_id(wayland.xdg_toplevel, "wayland-game-linux");

    wl_surface_commit(wayland.surface);
    wl_display_roundtrip(wayland.display);

    wayland.is_running = true;
    wayland.is_focused = true;

    return wayland;
}

// NOTE: audio
// how it comes that alsa does not need to be ran on separate thread and the audio works perfectly fine when resizing/moving the
// window?
// it still will be on separate thread though. just in case.

void audio_pause(linux_audio *audio) {
    if (!audio || !audio->pcm_handle) {
        return;
    }

    audio->is_paused = true;

    snd_pcm_drop(audio->pcm_handle);
}

void audio_resume(linux_audio *audio) {
    if (!audio || !audio->pcm_handle) {
        return;
    }

    snd_pcm_prepare(audio->pcm_handle);

    audio->is_paused = false;
}

void audio_update(linux_audio *audio) {
    if (!audio || !audio->pcm_handle) {
        return;
    }

    snd_pcm_sframes_t available_frames = snd_pcm_avail_update(audio->pcm_handle);

    if (available_frames < 0) {
        if (available_frames == -EPIPE) {
            snd_pcm_prepare(audio->pcm_handle);
        }

        return;
    }

    if (available_frames == 0) {
        return;
    }

    if (available_frames > audio->buffer_frame_count) {
        available_frames = audio->buffer_frame_count;
    }

    u32 bytes_per_frame = audio->channels * sizeof(f32);
    // TODO: DO NOT ALLOCATE!
    f32 *audio_data = (f32 *)malloc(available_frames * bytes_per_frame);

    if (!audio_data) {
        return;
    }

    f32 *sample_pointer = audio_data;

    for (u32 frame_index = 0; frame_index < available_frames; frame_index++) {
        f32 sample_value = (audio->phase < 0.5f) ? 0.05f : -0.05f;

        audio->phase += audio->phase_increment;
        if (audio->phase > 1.0f) {
            audio->phase -= 1.0f;
        }

        for (u32 channel_index = 0; channel_index < audio->channels; channel_index++) {
            *sample_pointer++ = sample_value;
        }
    }

    snd_pcm_sframes_t frames_written = snd_pcm_writei(audio->pcm_handle, audio_data, available_frames);
    if (frames_written < 0) {
        snd_pcm_prepare(audio->pcm_handle);
    }

    free(audio_data);
}

void *audio_thread_routine(void *thread_parameter) {
    linux_audio *audio = (linux_audio *)thread_parameter;

    while (audio->is_running) {
        if (!audio->is_paused) {
            audio_update(audio);
        }

        usleep(10000);
    }

    return 0;
}

bool audio_initialize(linux_audio *audio) {
    if (!audio) {
        return false;
    }

    memory_zero(audio, sizeof(linux_audio));

    audio->samples_per_second = 48000;
    audio->channels = 2;
    audio->phase_increment = 440.0f / (f32)audio->samples_per_second;

    int error_code = snd_pcm_open(&audio->pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (error_code < 0) {
        fprintf(stderr, "snd_pcm_open: %s\n", snd_strerror(error_code));

        return false;
    }

    snd_pcm_hw_params_t *hardware_parameters;
    snd_pcm_hw_params_alloca(&hardware_parameters);
    snd_pcm_hw_params_any(audio->pcm_handle, hardware_parameters);

    snd_pcm_hw_params_set_access(audio->pcm_handle, hardware_parameters, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(audio->pcm_handle, hardware_parameters, SND_PCM_FORMAT_FLOAT_LE);
    snd_pcm_hw_params_set_channels(audio->pcm_handle, hardware_parameters, audio->channels);
    snd_pcm_hw_params_set_rate_near(audio->pcm_handle, hardware_parameters, &audio->samples_per_second, 0);

    error_code = snd_pcm_hw_params(audio->pcm_handle, hardware_parameters);
    if (error_code < 0) {
        fprintf(stderr, "snd_pcm_hw_params: %s\n", snd_strerror(error_code));

        return false;
    }

    snd_pcm_hw_params_get_buffer_size(hardware_parameters, (snd_pcm_uframes_t *)&audio->buffer_frame_count);

    snd_pcm_prepare(audio->pcm_handle);

    audio->is_running = true;
    audio->is_paused = false;

    if (pthread_create(&audio->thread_handle, 0, audio_thread_routine, audio) != 0) {
        fprintf(stderr, "could not start audio thread.\n");
    }

    return true;
}

void memory_dump_standard_streams(game_memory *game_memory) {
    if (!game_memory) {
        return;
    }

    if (game_memory->standard_info_stream && game_memory->standard_info_stream->offset > 0) {
        isize bytes_written = write(STDOUT_FILENO, game_memory->standard_info_stream->memory, game_memory->standard_info_stream->offset);
        UNUSED(bytes_written);

        game_memory->standard_info_stream->offset = 0;
    }

    if (game_memory->standard_error_stream && game_memory->standard_error_stream->offset > 0) {
        isize bytes_written = write(STDERR_FILENO, game_memory->standard_error_stream->memory, game_memory->standard_error_stream->offset);
        UNUSED(bytes_written);

        game_memory->standard_error_stream->offset = 0;
    }
}

void run_update(linux_wayland *wayland, linux_audio *audio, vulkan *vulkan, game_memory *game_memory, render_command_buffer *command_buffer) {
    if (!wayland || !wayland->display) {
        return;
    }

    bool was_focused = wayland->is_focused;

    struct pollfd pollfd;
    pollfd.fd = wl_display_get_fd(wayland->display);
    pollfd.events = POLLIN;

    while (wayland->is_running) {
        memory_dump_standard_streams(game_memory);

        wl_display_dispatch_pending(wayland->display);
        wl_display_flush(wayland->display);

        if (poll(&pollfd, 1, 0) > 0) {
            wl_display_dispatch(wayland->display);
        }

        if (!wayland->is_focused && was_focused) {
            audio_pause(audio);
        } else if (wayland->is_focused && !was_focused) {
            audio_resume(audio);
        }

        was_focused = wayland->is_focused;

        if (wayland->is_focused) {
            render_command_buffer_reset(command_buffer);

            if (GAME_UPDATE_AND_RENDER) {
                GAME_UPDATE_AND_RENDER(game_memory, command_buffer);
            }

            if (!vulkan_frame_begin(vulkan, wayland, command_buffer)) {
                continue;
            }
            if (!vulkan_frame_end(vulkan, command_buffer)) {
                exit(1);
            }
        } else {
            usleep(100000);
        }
    }
}

int main(void) {
    linux_wayland wayland = window_create("linux wayland");

    if (!wayland.display) {
        return 1;
    }

    linux_audio audio;
    if (!audio_initialize(&audio)) {
        return 1;
    }

    usize permanent_arena_size = MEGABYTES(64);
    usize temporary_arena_size = MEGABYTES(256);

    void *permanent_memory_block = malloc(permanent_arena_size);
    void *temporary_memory_block = malloc(temporary_arena_size);

    memory_arena permanent_arena;
    memory_arena_initialize(&permanent_arena, permanent_memory_block, permanent_arena_size);

    memory_arena temporary_arena;
    memory_arena_initialize(&temporary_arena, temporary_memory_block, temporary_arena_size);

    usize error_stream_size = KILOBYTES(64);
    usize info_stream_size = KILOBYTES(256);

    memory_stream *error_stream = MEMORY_ARENA_PUSH_ARRAY(&permanent_arena, memory_stream, 1);
    memory_stream *info_stream = MEMORY_ARENA_PUSH_ARRAY(&permanent_arena, memory_stream, 1);

    memory_stream_initialize_writable(error_stream, MEMORY_ARENA_PUSH_BYTES(&permanent_arena, error_stream_size), error_stream_size);
    memory_stream_initialize_writable(info_stream, MEMORY_ARENA_PUSH_BYTES(&permanent_arena, info_stream_size), info_stream_size);

    vulkan vulkan;
    if (!vulkan_initialize(&vulkan, &wayland, info_stream, error_stream)) {
        return 1;
    }

    render_command_buffer *command_buffer = MEMORY_ARENA_PUSH_ARRAY(&permanent_arena, render_command_buffer, 1);

    usize command_buffer_size = MEGABYTES(2);
    void *command_buffer_memory = MEMORY_ARENA_PUSH_BYTES(&permanent_arena, command_buffer_size);
    render_command_buffer_initialize(command_buffer, command_buffer_memory, command_buffer_size);

    game_memory game_memory = {0};
    game_memory.standard_error_stream = error_stream;
    game_memory.standard_info_stream = info_stream;
    game_memory.permanent_arena = permanent_arena;
    game_memory.temporary_arena = temporary_arena;
    game_memory.is_initialized = false;

#if defined(DEBUG)
    game_code_load();
#endif

    run_update(&wayland, &audio, &vulkan, &game_memory, command_buffer);

    return 0;
}
