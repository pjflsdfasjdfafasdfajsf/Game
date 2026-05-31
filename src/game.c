#include "game_platform.h"

UPDATE_AND_RENDER(UpdateAndRender) {
    if (!memory->isInitialized) {
        MemoryStreamWriteString(memory->standardInfoStream, "Game says hi!");
        
        memory->isInitialized = true;
    }

    RenderClearEntireScreen(commandBuffer, Black);
    RenderDrawRectangle(commandBuffer, V2(10, 10), V2(20, 20), Red);
}

// Commented out temporarily until we have WAV parser so this sine wave does not bless our ears. But it works.
GET_SOUND_SAMPLES(GetSoundSamples) {
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
