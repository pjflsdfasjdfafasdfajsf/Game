// this is for compilers that do not support #embed such as MSVC.
// it allows for the same style usage:
//
// static const char data[] = {
// #include "generated_data.h"
// }
//
// (generated_data.h is just raw bytes.)
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

enum {
    read_buffer_size = 1 << 20
};

int main(int argument_count, const char *arguments[]) {
    if (argument_count != 3) {
        fprintf(stderr, "usage: %s <input_file_path> <output_file_path>\n", arguments[0]);

        return EXIT_FAILURE;
    }

    const char *input_file_path = arguments[1];
    const char *output_file_path = arguments[2];

    // clock_t start_time = clock();

    FILE *input_file = fopen(input_file_path, "rb");
    if (!input_file) {
        fprintf(stderr, "error: could not open input file: '%s'\n", input_file_path);

        return EXIT_FAILURE;
    }

    FILE *output_file = fopen(output_file_path, "wb");
    if (!output_file) {
        fprintf(stderr, "error: could not open output file: '%s'\n", output_file_path);

        return EXIT_FAILURE;
    }

    const size_t bytes_per_line = 64;
    static const char hex_digits[] = "0123456789ABCDEF";

    uint8_t *read_buffer = malloc(read_buffer_size);
    char *write_buffer = malloc((size_t)read_buffer_size * 6 + 64);

    if (!read_buffer || !write_buffer) {
        fprintf(stderr, "error: out of memory.\n");

        return EXIT_FAILURE;
    }

    size_t total_bytes_written = 0;
    size_t column = 0;

    for (;;) {
        size_t bytes_read_count = fread(read_buffer, 1, read_buffer_size, input_file);
        if (bytes_read_count == 0) {
            break;
        }

        char *write_cursor = write_buffer;

        for (size_t byte_index = 0; byte_index < bytes_read_count; byte_index++) {
            if (column == bytes_per_line) {
                *write_cursor++ = '\n';
                column = 0;
            }

            uint8_t byte = read_buffer[byte_index];

            *write_cursor++ = '0';
            *write_cursor++ = 'x';
            *write_cursor++ = hex_digits[byte >> 4];
            *write_cursor++ = hex_digits[byte & 0x0F];
            *write_cursor++ = ',';

            column++;
            total_bytes_written++;
        }

        size_t output_bytes_count = (size_t)(write_cursor - write_buffer);
        if (fwrite(write_buffer, 1, output_bytes_count, output_file) != output_bytes_count) {
            fprintf(stderr, "error: failed while writing output file: '%s'\n", output_file_path);

            return EXIT_FAILURE;
        }
    }

    if (ferror(input_file)) {
        fprintf(stderr, "error: failed while reading input file: '%s'\n", input_file_path);

        return EXIT_FAILURE;
    }

    if (total_bytes_written > 0) {
        fwrite("\n", 1, 1, output_file);
    }

    free(read_buffer);
    free(write_buffer);

    fclose(input_file);

    if (fclose(output_file) != 0) {
        fprintf(stderr, "error: failed while closing output file: '%s'\n", output_file_path);

        return EXIT_FAILURE;
    }

    // clock_t end_time = clock();
    // double time_taken_ms = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;

    // printf("%s: processed %zu bytes in %.2f ms.\n", input_file_path, total_bytes_written, time_taken_ms);

    return EXIT_SUCCESS;
}
