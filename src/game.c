#include "game_platform.h"
#include "game_png.h"

#if defined(WIN32)
#define EXPORTED __declspec(dllexport)
#elif defined(LINUX)
#define EXPORTED __attribute__((visibility("default")))
#else
#warning "EXPORTED macro was not defined."
#define EXPORTED
#endif

EXPORTED UPDATE_AND_RENDER(UpdateAndRender) {
    if (!memory->isInitialized) {
        MemoryStreamWriteLine(memory->standardInfoStream, "Game says hi!");

        static const char watermelon[] = {
#include "watermelon.png.h"
        };

        Image image = ImageLoadFromPNG(&memory->permanentArena, &memory->temporaryArena, memory->standardErrorStream, watermelon, sizeof(watermelon));
        RenderAllocateTexture(commandBuffer, 1, image.size, image.bytesPerPixel, image.pixels);

        memory->isInitialized = true;
    }

    RenderClearEntireScreen(commandBuffer, Black);
    RenderDrawRectangle(commandBuffer, V2(10, 10), V2(200, 200), Red, 1);
}

// Commented out temporarily until we have WAV parser so this sine wave does not bless our ears. But it works.
EXPORTED GET_SOUND_SAMPLES(GetSoundSamples) {
    Unused(audioBuffer);

    // // TODO: Move this into GameState later!
    // static f32 phase = 0.0f;

    // f32 phaseIncrement = 440.0f / (f32)audioBuffer->samplesPerSecond;
    // f32 *samplePointer = audioBuffer->samples;

    // for (u32 frameIndex = 0; frameIndex < audioBuffer->frameCount; frameIndex++) {
    //     f32 sampleValue = (phase < 0.5f) ? 0.05f : -0.05f;

    //     phase += phaseIncrement;
    //     if (phase > 1.0f) {
    //         phase -= 1.0f;
    //     }

    //     for (u32 channelIndex = 0; channelIndex < audioBuffer->channelCount; channelIndex++) {
    //         *samplePointer++ = sampleValue;
    //     }
    // }
}
