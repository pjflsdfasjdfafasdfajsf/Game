// TODO:
//     * Simple Vulkan renderer for a triangle
//     * Audio
//     * Image rendering
//     * Font rendering
//

#include "linux.h"
#include "game_platform.h"

// TODO: Can we get rid of it?
#include <X11/X.h>
#include <X11/Xlib.h>
#include <alsa/asoundlib.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

LinuxX11 WindowCreate(const char *title) {
    LinuxX11 x11;
    MemoryZero(&x11, sizeof(x11));

    Display *display = XOpenDisplay(0);
    if (!display) {
        fprintf(stderr, "Could not open X11 display.\n");

        return x11;
    }

    int defaultScreen = DefaultScreen(display);
    Window rootWindow = RootWindow(display, defaultScreen);

    unsigned long blackPixel = BlackPixel(display, defaultScreen);
    unsigned long whitePixel = WhitePixel(display, defaultScreen);

    Window window = XCreateSimpleWindow(display, rootWindow, 0, 0, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, 1, blackPixel, whitePixel);
    if (!window) {
        fprintf(stderr, "Could not create X11 window.\n");

        return x11;
    }

    XStoreName(display, window, title);

    Atom wmDeleteMessage = XInternAtom(display, "WM_DELETE_WINDOW", false);
    if (!XSetWMProtocols(display, window, &wmDeleteMessage, 1)) {
        fprintf(stderr, "Could not set WM_DELETE_WINDOW protocol.\n");

        return x11;
    }

    long eventMask = ExposureMask | KeyPressMask | KeyReleaseMask | FocusChangeMask | StructureNotifyMask;
    XSelectInput(display, window, eventMask);

    x11.display = display;
    x11.window = window;
    x11.wmDeleteMessage = wmDeleteMessage;

    return x11;
}

void WindowShow(LinuxX11 *x11) {
    if (!x11 || !x11->display) {
        return;
    }

    XMapWindow(x11->display, x11->window);
    XFlush(x11->display);
}

// NOTE: Audio
// How it comes that Alsa does not need to be ran on separate thread and the audio works perfectly fine when resizing/moving the
// window?
// It still will be on separate thread though. Just in case.

void AudioPause(LinuxAudio *audio) {
    if (!audio || !audio->pcmHandle) {
        return;
    }

    audio->isPaused = true;

    snd_pcm_drop(audio->pcmHandle);
}

void AudioResume(LinuxAudio *audio) {
    if (!audio || !audio->pcmHandle) {
        return;
    }

    snd_pcm_prepare(audio->pcmHandle);

    audio->isPaused = false;
}

void AudioUpdate(LinuxAudio *audio) {
    if (!audio || !audio->pcmHandle) {
        return;
    }

    snd_pcm_sframes_t availableFrames = snd_pcm_avail_update(audio->pcmHandle);

    if (availableFrames < 0) {
        if (availableFrames == -EPIPE) {
            snd_pcm_prepare(audio->pcmHandle);
        }

        return;
    }

    if (availableFrames == 0) {
        return;
    }

    if (availableFrames > audio->bufferFrameCount) {
        availableFrames = audio->bufferFrameCount;
    }

    u32 bytesPerFrame = audio->channels * sizeof(f32);
    f32 *audioData = (f32 *)malloc(availableFrames * bytesPerFrame);

    if (!audioData) {
        return;
    }

    f32 *samplePointer = audioData;

    for (u32 frameIndex = 0; frameIndex < availableFrames; frameIndex++) {
        f32 sampleValue = (audio->phase < 0.5f) ? 0.05f : -0.05f;

        audio->phase += audio->phaseIncrement;
        if (audio->phase > 1.0f) {
            audio->phase -= 1.0f;
        }

        for (u32 channelIndex = 0; channelIndex < audio->channels; channelIndex++) {
            *samplePointer++ = sampleValue;
        }
    }

    snd_pcm_sframes_t framesWritten = snd_pcm_writei(audio->pcmHandle, audioData, availableFrames);
    if (framesWritten < 0) {
        snd_pcm_prepare(audio->pcmHandle);
    }
}

void *AudioThreadRoutine(void *threadParameter) {
    LinuxAudio *audio = (LinuxAudio *)threadParameter;

    while (audio->isRunning) {
        if (!audio->isPaused) {
            AudioUpdate(audio);
        }

        usleep(10000);
    }

    return 0;
}

void AudioInitialize(LinuxAudio *audio) {
    if (!audio) {
        return;
    }

    MemoryZero(audio, sizeof(LinuxAudio));

    audio->samplesPerSecond = 48000;
    audio->channels = 2;
    audio->phaseIncrement = 440.0f / (f32)audio->samplesPerSecond;

    int errorCode = snd_pcm_open(&audio->pcmHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (errorCode < 0) {
        fprintf(stderr, "snd_pcm_open: %s\n", snd_strerror(errorCode));

        return;
    }

    snd_pcm_hw_params_t *hardwareParameters;
    snd_pcm_hw_params_alloca(&hardwareParameters);
    snd_pcm_hw_params_any(audio->pcmHandle, hardwareParameters);

    snd_pcm_hw_params_set_access(audio->pcmHandle, hardwareParameters, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(audio->pcmHandle, hardwareParameters, SND_PCM_FORMAT_FLOAT_LE);
    snd_pcm_hw_params_set_channels(audio->pcmHandle, hardwareParameters, audio->channels);
    snd_pcm_hw_params_set_rate_near(audio->pcmHandle, hardwareParameters, &audio->samplesPerSecond, 0);

    errorCode = snd_pcm_hw_params(audio->pcmHandle, hardwareParameters);
    if (errorCode < 0) {
        fprintf(stderr, "snd_pcm_hw_params: %s\n", snd_strerror(errorCode));

        return;
    }

    snd_pcm_hw_params_get_buffer_size(hardwareParameters, (snd_pcm_uframes_t *)&audio->bufferFrameCount);

    snd_pcm_prepare(audio->pcmHandle);

    audio->isRunning = true;
    audio->isPaused = false;

    if (pthread_create(&audio->threadHandle, 0, AudioThreadRoutine, audio) != 0) {
        fprintf(stderr, "Could not start audio thread.\n");
    }
}

void RunDraw() {
    // TODO
}

void RunUpdate(LinuxX11 *x11, LinuxAudio *audio) {
    if (!x11 || !x11->display) {
        return;
    }

    bool isRunning = true;
    bool wasFocused = true;
    bool isFocused = true;

    while (isRunning) {
        while (XPending(x11->display) > 0) {
            XEvent event;
            XNextEvent(x11->display, &event);

            switch (event.type) {
            case ClientMessage: {
                if ((Atom)event.xclient.data.l[0] == x11->wmDeleteMessage) {
                    isRunning = false;
                }
            } break;

            case FocusIn: {
                isFocused = true;
            } break;

            case FocusOut: {
                isFocused = false;
            } break;
            }
        }

        if (!isFocused && wasFocused) {
            AudioPause(audio);
        } else if (isFocused && !wasFocused) {
            AudioResume(audio);
        }

        wasFocused = isFocused;

        if (isFocused) {
            RunDraw();
        } else {
            usleep(100000);
        }
    }
}

int main() {
    LinuxX11 x11 = WindowCreate("Linux");

    if (!x11.display) {
        return 1;
    }

    LinuxAudio audio;
    AudioInitialize(&audio);

    WindowShow(&x11);

    RunUpdate(&x11, &audio);
}
