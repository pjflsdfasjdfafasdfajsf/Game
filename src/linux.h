#pragma once

#include <alsa/asoundlib.h>

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

#include "game_platform.h"

typedef struct {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct xdg_wm_base *xdgWmBase;

    struct wl_surface *surface;
    struct xdg_surface *xdgSurface;
    struct xdg_toplevel *xdgToplevel;

    bool isRunning;
    bool isFocused;
    bool isConfigured;
} LinuxWayland;

typedef struct {
    snd_pcm_t *pcmHandle;
    u32 samplesPerSecond;
    u32 channels;
    u32 bufferFrameCount;
 
    f32 phase;
    f32 phaseIncrement;

    bool isRunning;
    bool isPaused;
    pthread_t threadHandle;
} LinuxAudio;
