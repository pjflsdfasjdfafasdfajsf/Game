#pragma once

#include <X11/Xlib.h>
#include <alsa/asoundlib.h>

#include "game_platform.h"

typedef struct {
    Display *display;
    Window window;
    Atom wmDeleteMessage;
} LinuxX11;

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

