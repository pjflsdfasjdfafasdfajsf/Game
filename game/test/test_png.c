#include "utilities/init.h"
#include "utilities/file.h"
#include "utilities/ppm.h"

#include "game_png.h"

#if defined(FUZZING)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    UtilitiesInitialize();
    ImageLoadFromPNG(&permanentArena, &temporaryArena, &errorStream, data, size);

    return 0;
}

#else ///////////////////////////////////////////////////////

int main(void) {
    UtilitiesInitialize();

    usize fileSize = 0;
    void *fileData = ReadEntireFile("../../assets/images/watermelon.png", &fileSize);

    if (!fileData) {
        printf("game assets missing or running from the wrong directory\n");

        return 1;
    }

    Image result = ImageLoadFromPNG(&permanentArena, &temporaryArena, &errorStream, fileData, fileSize);

    if (result.pixels) {
        if (PPMWrite("png.ppm", result.size.width, result.size.height, result.bytesPerPixel, result.pixels)) {
            printf("wrote png.ppm\n");
        } else {
            printf("could not write png.ppm\n");
        }
    } else {
        printf("could not parse png\n");
        printf("error stream: %.*s\n", (int)errorStream.offset, errorStream.memory);
    }

    return 0;
}

#endif
