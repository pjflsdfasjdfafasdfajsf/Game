#include <stdio.h>
#include <stdlib.h>

#include "game_types.h"

static inline void *read_entire_file(const char *filename, usize *out_size) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }

    fseek(file, 0, SEEK_END);
    *out_size = (usize)ftell(file);
    fseek(file, 0, SEEK_SET);

    void *data = malloc(*out_size);
    if (data) {
        fread(data, 1, *out_size, file);
    }
    fclose(file);

    return data;
}
