#include <alsa/asoundlib.h>
#include <sys/stat.h>
#include <pthread.h>
#include <linux/limits.h>
#include <dlfcn.h>
#include <time.h>
#include <linux/input-event-codes.h>

#include "linux.h"
#include "linux_vulkan.h"
#include "game_platform.h"
#include "game_types.h"
#include "game.h"

static const char *global_application_name = "Game";
static const char *global_application_id = "cheesecake.game";

////////////////////////////////////////////
// NOTE: xdg window listener
////////////////////////////////////////////

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, u32 serial) {
    UNUSED(data);

    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    xdg_wm_base_ping};

///

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, u32 serial) {
    UNUSED(data);
    xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener xdg_surface_listener = {
    xdg_surface_configure};

///

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *xdg_toplevel, i32 width, i32 height, struct wl_array *states) {
    UNUSED(xdg_toplevel);
    UNUSED(states);

    linux_wayland *wayland = (linux_wayland *)data;
    ASSERT(wayland);

    if (width > 0 && height > 0) {
        wayland->width = width;
        wayland->height = height;
    }
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *xdg_toplevel) {
    UNUSED(xdg_toplevel);

    linux_wayland *wayland = (linux_wayland *)data;
    ASSERT(wayland);

    wayland->is_running = false;
}

static void xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *xdg_toplevel, i32 width, i32 height) {
    UNUSED(data);
    UNUSED(xdg_toplevel);
    UNUSED(width);
    UNUSED(height);
}

static void xdg_toplevel_wm_capabilities(void *data, struct xdg_toplevel *xdg_toplevel, struct wl_array *capabilities) {
    UNUSED(data);
    UNUSED(xdg_toplevel);
    UNUSED(capabilities);
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    xdg_toplevel_configure,
    xdg_toplevel_close,
    xdg_toplevel_configure_bounds,
    xdg_toplevel_wm_capabilities,
};

////////////////////////////////////////////
// NOTE: input listeners
////////////////////////////////////////////

static void pointer_enter(void *data, struct wl_pointer *pointer, u32 serial, struct wl_surface *surface, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    UNUSED(pointer);
    UNUSED(surface);
    UNUSED(surface_x);
    UNUSED(surface_y);

    linux_wayland *wayland = (linux_wayland *)data;
    ASSERT(wayland);

    if (wayland->cursor_shape_device) {
        wp_cursor_shape_device_v1_set_shape(wayland->cursor_shape_device, serial, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT);
    }
}

static void pointer_leave(void *data, struct wl_pointer *pointer, u32 serial, struct wl_surface *surface) {
    UNUSED(data);
    UNUSED(pointer);
    UNUSED(serial);
    UNUSED(surface);
}

static void pointer_motion(void *data, struct wl_pointer *pointer, u32 time, wl_fixed_t surface_x, wl_fixed_t surface_y) {
    UNUSED(time);
    UNUSED(pointer);
    
    linux_wayland *wayland = (linux_wayland *)data;
    ASSERT(wayland);

    game_input *game_input = &wayland->game_input;

    game_input->mouse_position.x = surface_x;
    game_input->mouse_position.y = surface_y;
}

static void pointer_button(void *data, struct wl_pointer *pointer, u32 serial, u32 time, u32 button_code, u32 state) {
    UNUSED(pointer);
    UNUSED(serial);
    UNUSED(time);
    
    linux_wayland *wayland = (linux_wayland *)data;
    ASSERT(wayland);

    game_input *game_input = &wayland->game_input;

    // just to make the compiler happy it is used in the switch
    UNUSED(game_input);

    game_button *button = 0;

    switch (button_code) {
#define BUTTON(name, linux_name) \
    case BTN_##linux_name: \
    { \
        button = &game_input->name; \
    } break;
    BUTTONS
#undef BUTTON
    }

    bool ended_down = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    if (button && button->ended_down != ended_down) {
        button->ended_down = ended_down;
        button->half_transition_count++;
    }
}

static void pointer_axis(void *data, struct wl_pointer *pointer, u32 time, u32 axis, wl_fixed_t value) {
    UNUSED(data);
    UNUSED(pointer);
    UNUSED(time);
    UNUSED(axis);
    UNUSED(value);
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_enter,
    pointer_leave,
    pointer_motion,
    pointer_button,
    pointer_axis,
    0,
    0,
    0,
    0,
    0,
    0,
};

////////////////////////////////////////////
// NOTE: keyboard listener
////////////////////////////////////////////

static void keyboard_keymap(void *data, struct wl_keyboard *keyboard, u32 format, i32 fd, u32 size) {
    UNUSED(data);
    UNUSED(keyboard);
    UNUSED(format);
    UNUSED(size);
    close(fd);
}

static void keyboard_enter(void *data, struct wl_keyboard *keyboard, u32 serial, struct wl_surface *surface, struct wl_array *keys) {
    UNUSED(data);
    UNUSED(keyboard);
    UNUSED(serial);
    UNUSED(surface);
    UNUSED(keys);
}

static void keyboard_leave(void *data, struct wl_keyboard *keyboard, u32 serial, struct wl_surface *surface) {
    UNUSED(data);
    UNUSED(keyboard);
    UNUSED(serial);
    UNUSED(surface);
}

static void keyboard_key(void *data, struct wl_keyboard *keyboard, u32 serial, u32 time, u32 key, u32 state) {
    UNUSED(keyboard);
    UNUSED(serial);
    UNUSED(time);
    
    linux_wayland *wayland = (linux_wayland *)data;
    ASSERT(wayland);

    game_input *game_input = &wayland->game_input;

    // just to make the compiler happy it is used in the switch
    UNUSED(game_input);
    
    game_button *button = 0;

    switch (key) {
#define KEY(lower, upper) \
    case KEY_##upper: \
    { \
        button = &game_input->lower; \
    } break;
    KEYS
#undef KEY
    }

    bool ended_down = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
    if (button && button->ended_down != ended_down) {
        button->ended_down = ended_down;
        button->half_transition_count++;
    }
}

static void keyboard_modifiers(void *data, struct wl_keyboard *keyboard, u32 serial, u32 mods_depressed, u32 mods_latched, u32 mods_locked, u32 group) {
    UNUSED(data);
    UNUSED(keyboard);
    UNUSED(serial);
    UNUSED(mods_depressed);
    UNUSED(mods_latched);
    UNUSED(mods_locked);
    UNUSED(group);
}

static void keyboard_repeat_info(void *data, struct wl_keyboard *keyboard, i32 rate, i32 delay) {
    UNUSED(data);
    UNUSED(keyboard);
    UNUSED(rate);
    UNUSED(delay);
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_keymap,
    keyboard_enter,
    keyboard_leave,
    keyboard_key,
    keyboard_modifiers,
    keyboard_repeat_info
};

///

static void seat_capabilities(void *data, struct wl_seat *seat, u32 capabilities) {
    linux_wayland *wayland = (linux_wayland *)data;
    ASSERT(wayland);

    bool has_pointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
    if (has_pointer && !wayland->pointer) {
        wayland->pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(wayland->pointer, &pointer_listener, wayland);

        if (wayland->cursor_shape_manager) {
            wayland->cursor_shape_device = wp_cursor_shape_manager_v1_get_pointer(wayland->cursor_shape_manager, wayland->pointer);
        }
    }

    bool has_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
    if (has_keyboard && !wayland->keyboard) {
        wayland->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(wayland->keyboard, &keyboard_listener, wayland);
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name) {
    UNUSED(data);
    UNUSED(seat);
    UNUSED(name);
}

static const struct wl_seat_listener seat_listener = {
    seat_capabilities,
    seat_name,
};

////////////////////////////////////////////
// NOTE: global listener
////////////////////////////////////////////

static void registry_handle_global(void *data, struct wl_registry *registry, u32 name, const char *interface, u32 version) {
    linux_wayland *wayland = (linux_wayland *)data;
    ASSERT(wayland);

#define BIND(member, struct_type, wayland_interface, maximum_supported_version)                              \
    if (strcmp(interface, wayland_interface.name) == 0) {                                                    \
        u32 bind_version = MIN(version, maximum_supported_version);                                          \
        wayland->member = (struct_type *)wl_registry_bind(registry, name, &wayland_interface, bind_version); \
        printf("using version %u of %s\n", bind_version, wayland_interface.name);                            \
    }

    BIND(compositor, struct wl_compositor, wl_compositor_interface, 4);
    BIND(xdg_wm_base, struct xdg_wm_base, xdg_wm_base_interface, 6);
    BIND(decoration_manager, struct zxdg_decoration_manager_v1, zxdg_decoration_manager_v1_interface, 1);
    BIND(seat, struct wl_seat, wl_seat_interface, 1);
    BIND(cursor_shape_manager, struct wp_cursor_shape_manager_v1, wp_cursor_shape_manager_v1_interface, 1);

#undef BIND
}

static void registry_handle_global_removal(void *data, struct wl_registry *registry, u32 name) {
    UNUSED(data);
    UNUSED(registry);
    UNUSED(name);
}

static const struct wl_registry_listener wayland_registry_listener = {
    registry_handle_global,
    registry_handle_global_removal,
};

////////////////////////////////////////////
// NOTE: windowing
////////////////////////////////////////////

static void linux_wayland_display_error(struct wl_display *display) {
    int error = wl_display_get_error(display);

    if (error == EPROTO) {
        u32 code = 0;
        const struct wl_interface *interface = 0;
        u32 id = wl_display_get_protocol_error(display, &interface, &code);

        fprintf(stderr, "wayland protocol error!\n");
        fprintf(stderr, "    interface: %s\n", interface ? interface->name : "unknown");
        fprintf(stderr, "    object ID: %u\n", id);
        fprintf(stderr, "    error code: %u\n", code);
    } else {
        fprintf(stderr, "error: wayland protocol communication error: %s\n", strerror(error));
    }
}

static bool linux_wayland_initialize(linux_wayland *wayland) {
    ASSERT(wayland);

    wayland->is_running = true;
    wayland->width = 1280;
    wayland->height = 720;

    wayland->display = wl_display_connect(0);
    if (!wayland->display) {
        fprintf(stderr, "error: could not connect to wayland display: %s\n", strerror(errno));

        return false;
    }

    struct wl_registry *registry = wl_display_get_registry(wayland->display);
    if (!registry) {
        // NOTE: this is very rare to fail unless its oom, hence the error message
        fprintf(stderr, "error: out of memory? (wl_display_get_registry)\n");

        return false;
    }

    wl_registry_add_listener(registry, &wayland_registry_listener, wayland);

    if (wl_display_roundtrip(wayland->display) == -1) {
        linux_wayland_display_error(wayland->display);

        return false;
    }

    if (!wayland->compositor || !wayland->xdg_wm_base) {
        fprintf(stderr, "error: wayland compositor is missing required extensions\n");

        return false;
    }

    xdg_wm_base_add_listener(wayland->xdg_wm_base, &xdg_wm_base_listener, wayland);

    wayland->surface = wl_compositor_create_surface(wayland->compositor);
    if (!wayland->surface) {
        fprintf(stderr, "error: could not create surface");

        return false;
    }

    struct xdg_surface *xdg_surface = xdg_wm_base_get_xdg_surface(wayland->xdg_wm_base, wayland->surface);
    if (!xdg_surface) {
        fprintf(stderr, "error: could not create xdg surface");

        return false;
    }
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, wayland);

    struct xdg_toplevel *xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    if (!xdg_toplevel) {
        fprintf(stderr, "error: could not get xdg toplevel");

        return false;
    }

    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, wayland);
    // NOTE: this is needed for some (?) DEs like KDE, where if the app ID is not set you
    // will not be able to resize the window
    xdg_toplevel_set_app_id(xdg_toplevel, global_application_id);
    xdg_toplevel_set_title(xdg_toplevel, global_application_name);

    if (wayland->decoration_manager) {
        struct zxdg_toplevel_decoration_v1 *decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(wayland->decoration_manager, xdg_toplevel);
        zxdg_toplevel_decoration_v1_set_mode(decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
    } else {
        fprintf(stderr, "warning: compositor does not support server-side decorations\n");
    }

    if (wayland->seat) {
        wl_seat_add_listener(wayland->seat, &seat_listener, wayland);
    }

    wl_surface_commit(wayland->surface);
    wl_display_roundtrip(wayland->display);

    return true;
}

////////////////////////////////////////////
// NOTE: sound
////////////////////////////////////////////

// how it comes that alsa does not need to be ran on separate thread and the sound works perfectly
// fine when resizing/moving the window?
// it still will be on separate thread though. just in case.

void linux_sound_pause(linux_sound *sound) {
    if (!sound || !sound->pcm_handle) {
        return;
    }

    sound->is_paused = true;

    snd_pcm_drop(sound->pcm_handle);
}

void linux_sound_resume(linux_sound *sound) {
    if (!sound || !sound->pcm_handle) {
        return;
    }

    snd_pcm_prepare(sound->pcm_handle);

    sound->is_paused = false;
}

void linux_sound_update(linux_sound *sound) {
    if (!sound || !sound->pcm_handle) {
        return;
    }

    snd_pcm_sframes_t available_frames = snd_pcm_avail_update(sound->pcm_handle);

    if (available_frames < 0) {
        if (available_frames == -EPIPE) {
            snd_pcm_prepare(sound->pcm_handle);
        }

        return;
    }

    if (available_frames == 0) {
        return;
    }

    if (available_frames > sound->buffer_frame_count) {
        available_frames = sound->buffer_frame_count;
    }

    f32 *sound_data = sound->sample_buffer;

    if (sound->game_code->get_sound_samples) {
        sound_buffer buffer = {0};
        buffer.samples_per_second = sound->samples_per_second;
        buffer.channel_count = sound->channels;
        buffer.frame_count = available_frames;
        buffer.samples = sound_data;

        sound->game_code->get_sound_samples(&buffer);
    } else {
        u32 bytes_to_clear = available_frames * sound->channels * sizeof(f32);
        memory_zero(sound_data, bytes_to_clear);
    }

    snd_pcm_sframes_t frames_written = snd_pcm_writei(sound->pcm_handle, sound_data, available_frames);
    if (frames_written < 0) {
        snd_pcm_prepare(sound->pcm_handle);
    }
}

void *liux_sound_thread(void *thread_parameter) {
    linux_sound *sound = (linux_sound *)thread_parameter;

    while (sound->is_running) {
        if (!sound->is_paused) {
            linux_sound_update(sound);
        }

        usleep(10000);
    }

    return 0;
}

bool linux_sound_initialize(linux_sound *sound, linux_game_code *game_code, memory_arena *permanent_arena) {
    if (!sound) {
        return false;
    }

    memory_zero(sound, sizeof(linux_sound));

    sound->samples_per_second = 48000;
    sound->channels = 2;
    sound->game_code = game_code;

    int error_code = snd_pcm_open(&sound->pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (error_code < 0) {
        fprintf(stderr, "error: snd_pcm_open: %s\n", snd_strerror(error_code));

        return false;
    }

    snd_pcm_hw_params_t *hardware_parameters;
    snd_pcm_hw_params_alloca(&hardware_parameters);
    snd_pcm_hw_params_any(sound->pcm_handle, hardware_parameters);

    snd_pcm_hw_params_set_access(sound->pcm_handle, hardware_parameters, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(sound->pcm_handle, hardware_parameters, SND_PCM_FORMAT_FLOAT_LE);
    snd_pcm_hw_params_set_channels(sound->pcm_handle, hardware_parameters, sound->channels);
    snd_pcm_hw_params_set_rate_near(sound->pcm_handle, hardware_parameters, &sound->samples_per_second, 0);

    error_code = snd_pcm_hw_params(sound->pcm_handle, hardware_parameters);
    if (error_code < 0) {
        fprintf(stderr, "error: snd_pcm_hw_params: %s\n", snd_strerror(error_code));

        return false;
    }

    snd_pcm_hw_params_get_buffer_size(hardware_parameters, (snd_pcm_uframes_t *)&sound->buffer_frame_count);

    u32 bytes_per_frame = sound->channels * sizeof(u32);
    usize total_buffer_size = sound->buffer_frame_count * bytes_per_frame;

    sound->sample_buffer = (f32 *)MEMORY_ARENA_PUSH_BYTES(permanent_arena, total_buffer_size);
    if (!sound->sample_buffer) {
        fprintf(stderr, "error: out of memory\n");

        return false;
    }

    snd_pcm_prepare(sound->pcm_handle);

    sound->is_running = true;
    sound->is_paused = false;

    if (pthread_create(&sound->thread_handle, 0, liux_sound_thread, sound) != 0) {
        fprintf(stderr, "error: could not start sound thread.\n");
    }

    return true;
}

////////////////////////////////////////////

void linux_dump_standard_streams(game_memory *game_memory) {
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

static void linux_absolute_libary_path(const char *library_file_name, char *destination_buffer, usize destination_capacity) {
    memory_zero(destination_buffer, destination_capacity);

    char executable_path[PATH_MAX];
    isize executable_path_length = readlink("/proc/self/exe", executable_path, sizeof(executable_path) - 1);

    if (executable_path_length == -1) {
        printf("error: readlink failed.\n");

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

////////////////////////////////////////////

#if defined(DEBUG)

static bool linux_copy_file(const char *source_path, const char *destination_path) {
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

static time_t linux_get_last_write_time(const char *filename) {
    struct stat file_status;
    if (stat(filename, &file_status) == 0) {
        return file_status.st_mtime;
    }
    return 0;
}

static void linux_unload_game_code(linux_game_code *game_code) {
    if (game_code->handle) {
        dlclose(game_code->handle);
        game_code->handle = 0;
    }

    game_code->is_valid = false;

    game_code->update_and_render = 0;
    game_code->get_sound_samples = 0;
}

static linux_game_code linux_load_game_code(const char *source_so_name, const char *temp_so_name) {
    linux_game_code result = {0};

    result.last_write_time = linux_get_last_write_time(source_so_name);

    linux_copy_file(source_so_name, temp_so_name);
    result.handle = dlopen(temp_so_name, RTLD_NOW);

    if (result.handle) {
        result.update_and_render = (update_and_render_function *)dlsym(result.handle, "update_and_render");
        result.get_sound_samples = (get_sound_samples_function *)dlsym(result.handle, "get_sound_samples");

        result.is_valid = (result.update_and_render && result.get_sound_samples);
    }

    if (!result.is_valid) {
        result.update_and_render = 0;
        result.get_sound_samples = 0;
    }

    return result;
}

#endif

////////////////////////////////////////////

static void linux_update(linux_wayland *wayland, linux_sound *sound, linux_game_code *game_code, vulkan *vulkan, game_memory *game_memory, render_command_buffer *command_buffer) {
    UNUSED(sound); // NOTE: used in the debug build

    struct pollfd pollfd;
    pollfd.fd = wl_display_get_fd(wayland->display);
    pollfd.events = POLLIN;

    char source_path[PATH_MAX];
    char temporary_path[PATH_MAX];
    linux_absolute_libary_path("game.so", source_path, sizeof(source_path));
    linux_absolute_libary_path("game.temp.so", temporary_path, sizeof(temporary_path));

    struct timespec last_timespec;
    timespec_get(&last_timespec, TIME_UTC);
    struct timespec current_timespec;

    while (wayland->is_running) {
        timespec_get(&current_timespec, TIME_UTC);
        wayland->game_input.delta_time = MILLISECONDS(current_timespec.tv_nsec) - MILLISECONDS(last_timespec.tv_nsec);
        last_timespec = current_timespec;
        
        for (u32 i = 0; i < ARRAY_COUNT(wayland->game_input.buttons); i++) {
            wayland->game_input.buttons[i].half_transition_count = 0;
        }

#if defined(DEBUG)
        if (linux_get_last_write_time(source_path) != game_code->last_write_time) {
            sound->is_paused = true;

            linux_unload_game_code(game_code);
            *game_code = linux_load_game_code(source_path, temporary_path);

            sound->is_paused = false;
        }
#endif

        wl_display_dispatch_pending(wayland->display);
        wl_display_flush(wayland->display);

        if (poll(&pollfd, 1, 0) > 0) {
            wl_display_dispatch(wayland->display);
        }

        linux_dump_standard_streams(game_memory);
        render_command_buffer_reset(command_buffer);

        if (game_code->update_and_render) {
            game_code->update_and_render(game_memory, &wayland->game_input, command_buffer);
        }

        if (!vulkan_frame_begin(vulkan, wayland, command_buffer)) {
            continue;
        }

        if (!vulkan_frame_end(vulkan, command_buffer)) {
            exit(EXIT_FAILURE);
        }
    }
}

int main(void) {
    linux_wayland wayland = {0};
    if (!linux_wayland_initialize(&wayland)) {
        return EXIT_FAILURE;
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

    linux_game_code game_code = {0};

#if defined(DEBUG)
    char source_path[PATH_MAX];
    char temporary_path[PATH_MAX];
    linux_absolute_libary_path("game.so", source_path, sizeof(source_path));
    linux_absolute_libary_path("game.temp.so", temporary_path, sizeof(temporary_path));

    game_code = linux_load_game_code(source_path, temporary_path);
#else
    game_code.update_and_render = update_and_render;
    game_code.get_sound_samples = get_sound_samples;
    game_code.is_valid = true;
#endif

    linux_sound sound;
    if (!linux_sound_initialize(&sound, &game_code, &permanent_arena)) {
        return EXIT_FAILURE;
    }

    vulkan vulkan;
    if (!vulkan_initialize(&vulkan, &wayland, info_stream, error_stream)) {
        return EXIT_FAILURE;
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

    linux_update(&wayland, &sound, &game_code, &vulkan, &game_memory, command_buffer);

    return EXIT_SUCCESS;
}
