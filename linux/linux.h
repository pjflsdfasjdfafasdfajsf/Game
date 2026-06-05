#pragma once

#include <alsa/asoundlib.h>

#include <wayland-client.h>

#include "cursor-shape-v1-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "xdg-shell-client-protocol.h"

#include "game_types.h"

typedef struct {
    bool is_running;

    struct wl_display* display;
    struct wl_surface* surface;
    struct wl_keyboard *keyboard;

    struct wl_compositor* compositor;
    struct xdg_wm_base* xdg_wm_base;
    struct zxdg_decoration_manager_v1* decoration_manager;
    struct wl_seat* seat;
    struct wl_pointer* pointer;
    struct wp_cursor_shape_manager_v1* cursor_shape_manager;
    struct wp_cursor_shape_device_v1* cursor_shape_device;

    u32 width;
    u32 height;
} linux_wayland;

typedef struct {
    snd_pcm_t *pcm_handle;
    u32 samples_per_second;
    u32 channels;
    u32 buffer_frame_count;
 
    f32 *sample_buffer;

    bool is_running;
    bool is_paused;
    pthread_t thread_handle;
} linux_sound;
