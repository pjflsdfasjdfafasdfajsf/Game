#include "utilities/init.h"
#include "utilities/file.h"
#include "utilities/ppm.h"

#include "game_png.h"

#if defined(FUZZING)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    utilities_initialize();
    image_load_from_png(&permanent_arena, &temporary_arena, &error_stream, data, size);

    return 0;
}

#else ///////////////////////////////////////////////////////

int main(void) {
    utilities_initialize();

    usize file_size = 0;
    void *file_data = read_entire_file("data/image.png", &file_size);

    if (!file_data) {
        printf("test data missing\n");

        return 1;
    }

    image result = image_load_from_png(&permanent_arena, &temporary_arena, &error_stream, file_data, file_size);

    if (result.pixels) {
        if (ppm_write("png.ppm", result.size.width, result.size.height, result.bytes_per_pixel, result.pixels)) {
            printf("wrote png.ppm\n");
        } else {
            printf("could not write png.ppm\n");
        }
    } else {
        printf("could not parse png\n");
        printf("error stream: %.*s\n", (int)error_stream.offset, error_stream.memory);
    }

    return 0;
}

#endif
