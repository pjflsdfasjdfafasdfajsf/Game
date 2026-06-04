#include <stdio.h>
#include <stdlib.h>

#include "game_types.h"

static inline void *ReadEntireFile(const char *filename, usize *outSize) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    *outSize = (usize)ftell(file);
    fseek(file, 0, SEEK_SET);

    void *data = malloc(*outSize);
    if (data) {
        fread(data, 1, *outSize, file);
    }
    fclose(file);

    return data;
}
