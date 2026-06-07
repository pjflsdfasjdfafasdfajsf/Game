#include "utilities/init.h"
#include "utilities/file.h"

#include "game_wav.h"

#if defined(FUZZING)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    utilities_initialize();
    sound_load_from_wav(&permanent_arena, &error_stream, data, size);

    return 0;
}

#else ///////////////////////////////////////////////////////

int main(void) {
    utilities_initialize();

    usize file_size = 0;
    void *file_data = read_entire_file("data/sine_440.wav", &file_size);

    if (!file_data) {
        printf("test data missing\n");
        return 1;
    }

    sound_clip result = sound_load_from_wav(&permanent_arena, &error_stream, file_data, file_size);

    if (result.samples) {
        FILE *file = fopen("audio.raw", "wb");
        if (file) {
            usize bytes_per_sample = result.bits_per_sample / 8;
            usize total_bytes = result.sample_count * result.channel_count * bytes_per_sample;

            fwrite(result.samples, 1, total_bytes, file);
            fclose(file);

            printf("wrote audio.raw\n");
            printf("\n");

            const char *format = "s16le";
            if (result.bits_per_sample == 8) {
                format = "u8";
            } else if (result.bits_per_sample == 24) {
                format = "s24le";
            } else if (result.bits_per_sample == 32) {
                format = "s32le";
            }

            const char *layout = (result.channel_count == 1) ? "mono" : "stereo";

            printf("ffplay -f %s -ar %u -ch_layout %s audio.raw\n",format, result.sample_rate, layout);
        } else {
            printf("could not write audio_out.raw\n");
        }
    } else {
        printf("could not parse wav\n");
        printf("error stream: %.*s\n", (int)error_stream.offset, (char *)error_stream.memory);
    }

    free(file_data);

    return 0;
}

#endif
