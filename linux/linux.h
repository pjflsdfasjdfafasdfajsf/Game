#pragma once

#include <alsa/asoundlib.h>

#include <wayland-client.h>
#include <xdg-shell-client-protocol.h>

#include "game_platform.h"
#include "game_types.h"

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *xdg_wm_base;

    struct wl_surface *surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;

    u32 width;
    u32 height;

    bool is_running;
    bool is_focused;
    bool is_configured;
} linux_wayland;

typedef struct {
    snd_pcm_t *pcm_handle;
    u32 samples_per_second;
    u32 channels;
    u32 buffer_frame_count;
 
    f32 phase;
    f32 phase_increment;

    bool is_running;
    bool is_paused;
    pthread_t thread_handle;
} linux_audio;
