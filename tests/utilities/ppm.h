#include <stdio.h>

#include "game_types.h"

static inline bool ppm_write(const char *filename, u32 width, u32 height, u32 bytes_per_pixel, const u8 *pixels) {
    if (!filename || !width || !height || !bytes_per_pixel || !pixels) {
        return false;
    }

    FILE *file = fopen(filename, "wb");
    if (!file) {
        return false;
    }

    fprintf(file, "P6\n%u %u\n255\n", width, height);

    for (u32 i = 0; i < width * height; i++) {
        u8 r = 0, g = 0, b = 0;

        if (bytes_per_pixel == 1) {
            r = g = b = pixels[i];
        } else if (bytes_per_pixel == 2) {
            r = g = b = pixels[i * 2];
        } else if (bytes_per_pixel == 3) {
            r = pixels[i * 3 + 0];
            g = pixels[i * 3 + 1];
            b = pixels[i * 3 + 2];
        } else if (bytes_per_pixel == 4) {
            u8 alpha = pixels[i * 4 + 3];

            r = (pixels[i * 4 + 0] * alpha) / 255;
            g = (pixels[i * 4 + 1] * alpha) / 255;
            b = (pixels[i * 4 + 2] * alpha) / 255;
        }

        u8 rgb[3] = {r, g, b};
        fwrite(rgb, 1, 3, file);
    }

    fclose(file);
    return true;
}
