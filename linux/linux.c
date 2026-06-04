#include <alsa/asoundlib.h>
#include <pthread.h>
#include <limits.h>
#include <dlfcn.h>

#include "linux.h"
#include "linux_vulkan.h"
#include "game_platform.h"
#include "game_types.h"
#include "game.h"

#if defined(DEBUG)
static void *GameSO = 0;
static UpdateAndRenderFunction *GameUpdateAndRender = 0;
static GetSoundSamplesFunction *GameGetSoundSamples = 0;

static void AbsoluteLibaryPath(const char *libraryFileName, char *destinationBuffer, usize destinationCapacity) {
    MemoryZero(destinationBuffer, destinationCapacity);

    char executablePath[PATH_MAX];
    isize executablePathLength = readlink("/proc/self/exe", executablePath, sizeof(executablePath) - 1);

    if (executablePathLength == -1) {
        printf("ERROR: readlink failed.\n");

        return;
    }

    executablePath[executablePathLength] = '\0';

    isize finalSlashIndex = StringFindLastOccurrenceOfCharacter(executablePath, '/');

    if (finalSlashIndex != -1) {
        executablePath[finalSlashIndex + 1] = '\0';
    }

    StringCopy(destinationBuffer, destinationCapacity, executablePath);
    StringAppend(destinationBuffer, destinationCapacity, libraryFileName);
}

static bool CopyFile(const char *sourcePath, const char *destinationPath) {
    int sourceFile = open(sourcePath, O_RDONLY);
    if (sourceFile < 0) {
        return false;
    }

    int destinationFile = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (destinationFile < 0) {
        close(sourceFile);
        return false;
    }

    char buffer[4096];

    while (true) {
        isize bytesRead = read(sourceFile, buffer, sizeof(buffer));

        if (bytesRead == 0) {
            break;
        }

        if (bytesRead < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        char *writePointer = buffer;
        isize bytesRemaining = bytesRead;

        while (bytesRemaining > 0) {
            isize bytesWritten = write(destinationFile, writePointer, bytesRemaining);

            if (bytesWritten >= 0) {
                bytesRemaining -= bytesWritten;
                writePointer += bytesWritten;
            } else if (errno != EINTR) {
                return false;
            }
        }
    }

    close(sourceFile);

    if (close(destinationFile) < 0) {
        return false;
    }

    return true;
}

static void GameCodeLoad(void) {
    if (GameSO) {
        dlclose(GameSO);

        GameSO = 0;
        GameUpdateAndRender = 0;
        GameGetSoundSamples = 0;
    }

    char sourceLibraryPath[PATH_MAX];
    char temporaryLibraryPath[PATH_MAX];

    AbsoluteLibaryPath("../lib/Game.so", sourceLibraryPath, sizeof(sourceLibraryPath));
    AbsoluteLibaryPath("libgame.temp.so", temporaryLibraryPath, sizeof(temporaryLibraryPath));

    if (!CopyFile(sourceLibraryPath, temporaryLibraryPath)) {
        fprintf(stderr, "Could not copy Game.so.\n");
    }

    GameSO = dlopen(temporaryLibraryPath, RTLD_NOW);
    if (!GameSO) {
        fprintf(stderr, "%s\n", dlerror());
    }

    GameUpdateAndRender = (UpdateAndRenderFunction *)dlsym(GameSO, "UpdateAndRender");
    GameGetSoundSamples = (GetSoundSamplesFunction *)dlsym(GameSO, "GetSoundSamples");
}

#else
#define GameUpdateAndRender UpdateAndRender
#define GameGetSoundSamples GetSoundSamples
#endif

static void XdgToplevelConfigureHandler(void *userData, struct xdg_toplevel *xdgToplevel, int32_t width, int32_t height, struct wl_array *states) {
    Unused(xdgToplevel), Unused(width), Unused(height);

    LinuxWayland *wayland = (LinuxWayland *)userData;

    if (!wayland) {
        return;
    }

    bool isActivated = false;
    uint32_t *state;
    wl_array_for_each(state, states) {
        if (*state == XDG_TOPLEVEL_STATE_ACTIVATED) {
            isActivated = true;
        }
    }

    if (width > 0) {
        wayland->width = width;
    }
    if (height > 0) {
        wayland->height = height;
    }

    wayland->isFocused = isActivated;
}

static void XdgToplevelCloseHandler(void *userData, struct xdg_toplevel *xdgToplevel) {
    Unused(xdgToplevel);

    LinuxWayland *wayland = (LinuxWayland *)userData;

    if (wayland) {
        wayland->isRunning = false;
    }
}

static const struct xdg_toplevel_listener xdgToplevelListener = {
    .configure = XdgToplevelConfigureHandler,
    .close = XdgToplevelCloseHandler,
};

static void XdgSurfaceConfigureHandler(void *userData, struct xdg_surface *xdgSurface, uint32_t serial) {
    LinuxWayland *wayland = (LinuxWayland *)userData;

    xdg_surface_ack_configure(xdgSurface, serial);

    if (wayland) {
        wayland->isConfigured = true;
    }
}

static const struct xdg_surface_listener xdgSurfaceListener = {
    .configure = XdgSurfaceConfigureHandler,
};

static void XdgWindowManagerBasePingHandler(void *userData, struct xdg_wm_base *xdgWmBase, uint32_t serial) {
    Unused(userData);

    xdg_wm_base_pong(xdgWmBase, serial);
}

static const struct xdg_wm_base_listener xdgWmBaseListener = {
    .ping = XdgWindowManagerBasePingHandler,
};

static void RegistryGlobalHandler(void *userData, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version) {
    Unused(version);

    LinuxWayland *wayland = (LinuxWayland *)userData;

    usize compositorNameLength = StringGetLength(wl_compositor_interface.name);
    usize xdgWmBaseNameLength = StringGetLength(xdg_wm_base_interface.name);

    bool isCompositor = MemoryEquals(interface, wl_compositor_interface.name, compositorNameLength + 1);
    bool isXdgWmBase = MemoryEquals(interface, xdg_wm_base_interface.name, xdgWmBaseNameLength + 1);

    if (isCompositor) {
        wayland->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    } else if (isXdgWmBase) {
        wayland->xdgWmBase = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(wayland->xdgWmBase, &xdgWmBaseListener, wayland);
    }
}

static void RegistryGlobalRemoveHandler(void *userData, struct wl_registry *registry, uint32_t name) {
    Unused(userData), Unused(registry), Unused(name);
}

static const struct wl_registry_listener registryListener = {
    .global = RegistryGlobalHandler,
    .global_remove = RegistryGlobalRemoveHandler,
};

LinuxWayland WindowCreate(const char *title) {
    LinuxWayland wayland = {0};

    wayland.width = DEFAULT_WINDOW_WIDTH;
    wayland.height = DEFAULT_WINDOW_HEIGHT;

    wayland.display = wl_display_connect(0);
    if (!wayland.display) {
        fprintf(stderr, "Could not connect to Wayland display.\n");
        return wayland;
    }

    wayland.registry = wl_display_get_registry(wayland.display);
    wl_registry_add_listener(wayland.registry, &registryListener, &wayland);

    wl_display_roundtrip(wayland.display);

    if (!wayland.compositor || !wayland.xdgWmBase) {
        fprintf(stderr, "Failed to bind Wayland compositor or xdg_wm_base.\n");
        return wayland;
    }

    wayland.surface = wl_compositor_create_surface(wayland.compositor);
    wayland.xdgSurface = xdg_wm_base_get_xdg_surface(wayland.xdgWmBase, wayland.surface);
    xdg_surface_add_listener(wayland.xdgSurface, &xdgSurfaceListener, &wayland);

    wayland.xdgToplevel = xdg_surface_get_toplevel(wayland.xdgSurface);
    xdg_toplevel_add_listener(wayland.xdgToplevel, &xdgToplevelListener, &wayland);

    xdg_toplevel_set_title(wayland.xdgToplevel, title);
    xdg_toplevel_set_app_id(wayland.xdgToplevel, "wayland-game-linux");

    wl_surface_commit(wayland.surface);
    wl_display_roundtrip(wayland.display);

    wayland.isRunning = true;
    wayland.isFocused = true;

    return wayland;
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
    // TODO: DO NOT ALLOCATE!
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

    free(audioData);
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

bool AudioInitialize(LinuxAudio *audio) {
    if (!audio) {
        return false;
    }

    MemoryZero(audio, sizeof(LinuxAudio));

    audio->samplesPerSecond = 48000;
    audio->channels = 2;
    audio->phaseIncrement = 440.0f / (f32)audio->samplesPerSecond;

    int errorCode = snd_pcm_open(&audio->pcmHandle, "default", SND_PCM_STREAM_PLAYBACK, 0);
    if (errorCode < 0) {
        fprintf(stderr, "snd_pcm_open: %s\n", snd_strerror(errorCode));

        return false;
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

        return false;
    }

    snd_pcm_hw_params_get_buffer_size(hardwareParameters, (snd_pcm_uframes_t *)&audio->bufferFrameCount);

    snd_pcm_prepare(audio->pcmHandle);

    audio->isRunning = true;
    audio->isPaused = false;

    if (pthread_create(&audio->threadHandle, 0, AudioThreadRoutine, audio) != 0) {
        fprintf(stderr, "Could not start audio thread.\n");
    }

    return true;
}

void MemoryDumpStandardStreams(GameMemory *gameMemory) {
    if (!gameMemory) {
        return;
    }

    if (gameMemory->standardInfoStream && gameMemory->standardInfoStream->offset > 0) {
        isize bytesWritten = write(STDOUT_FILENO, gameMemory->standardInfoStream->memory, gameMemory->standardInfoStream->offset);
        Unused(bytesWritten);

        gameMemory->standardInfoStream->offset = 0;
    }

    if (gameMemory->standardErrorStream && gameMemory->standardErrorStream->offset > 0) {
        isize bytesWritten = write(STDERR_FILENO, gameMemory->standardErrorStream->memory, gameMemory->standardErrorStream->offset);
        Unused(bytesWritten);

        gameMemory->standardErrorStream->offset = 0;
    }
}

void RunUpdate(LinuxWayland *wayland, LinuxAudio *audio, Vulkan *vulkan, GameMemory *gameMemory, RenderCommandBuffer *commandBuffer) {
    if (!wayland || !wayland->display) {
        return;
    }

    bool wasFocused = wayland->isFocused;

    struct pollfd pollfd;
    pollfd.fd = wl_display_get_fd(wayland->display);
    pollfd.events = POLLIN;

    while (wayland->isRunning) {
        MemoryDumpStandardStreams(gameMemory);

        wl_display_dispatch_pending(wayland->display);
        wl_display_flush(wayland->display);

        if (poll(&pollfd, 1, 0) > 0) {
            wl_display_dispatch(wayland->display);
        }

        if (!wayland->isFocused && wasFocused) {
            AudioPause(audio);
        } else if (wayland->isFocused && !wasFocused) {
            AudioResume(audio);
        }

        wasFocused = wayland->isFocused;

        if (wayland->isFocused) {
            RenderCommandBufferReset(commandBuffer);

            if (GameUpdateAndRender) {
                GameUpdateAndRender(gameMemory, commandBuffer);
            }

            if (!VulkanFrameBegin(vulkan, wayland, commandBuffer)) {
                continue;
            }
            if (!VulkanFrameEnd(vulkan, commandBuffer)) {
                exit(1);
            }
        } else {
            usleep(100000);
        }
    }
}

int main(void) {
    LinuxWayland wayland = WindowCreate("Linux Wayland");

    if (!wayland.display) {
        return 1;
    }

    LinuxAudio audio;
    if (!AudioInitialize(&audio)) {
        return 1;
    }

    Vulkan vulkan;
    if (!VulkanInitialize(&vulkan, &wayland)) {
        return 1;
    }

    usize permanentArenaSize = Megabytes(64);
    usize temporaryArenaSize = Megabytes(256);

    void *permanentMemoryBlock = malloc(permanentArenaSize);
    void *temporaryMemoryBlock = malloc(temporaryArenaSize);

    MemoryArena permanentArena;
    MemoryArenaInitialize(&permanentArena, permanentMemoryBlock, permanentArenaSize);

    MemoryArena temporaryArena;
    MemoryArenaInitialize(&temporaryArena, temporaryMemoryBlock, temporaryArenaSize);

    usize errorStreamSize = Kilobytes(64);
    usize infoStreamSize = Kilobytes(256);

    MemoryStream *errorStream = MemoryArenaPushArray(&permanentArena, MemoryStream, 1);
    MemoryStream *infoStream = MemoryArenaPushArray(&permanentArena, MemoryStream, 1);

    MemoryStreamInitializeWritable(errorStream, MemoryArenaPushBytes(&permanentArena, errorStreamSize), errorStreamSize);
    MemoryStreamInitializeWritable(infoStream, MemoryArenaPushBytes(&permanentArena, infoStreamSize), infoStreamSize);

    RenderCommandBuffer *commandBuffer = MemoryArenaPushArray(&permanentArena, RenderCommandBuffer, 1);

    usize commandBufferSize = Megabytes(2);
    void *commandBufferMemory = MemoryArenaPushBytes(&permanentArena, commandBufferSize);
    RenderCommandBufferInitialize(commandBuffer, commandBufferMemory, commandBufferSize);

    GameMemory gameMemory = {0};
    gameMemory.standardErrorStream = errorStream;
    gameMemory.standardInfoStream = infoStream;
    gameMemory.permanentArena = permanentArena;
    gameMemory.temporaryArena = temporaryArena;
    gameMemory.isInitialized = false;

#if defined(DEBUG)
    GameCodeLoad();
#endif

    RunUpdate(&wayland, &audio, &vulkan, &gameMemory, commandBuffer);

    return 0;
}
